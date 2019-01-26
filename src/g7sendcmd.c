/* =========================================================================
 * File:        g7sendcmd.c
 * Description: Low/mid level routines to send command to device
 * Author:      Johan Persson (johan162@gmail.com)
 *
 * Copyright (C) 2013-2015 Johan Persson
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 * =========================================================================
 */

// We want the full POSIX and C99 standard
#define _GNU_SOURCE

// Standard defines
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <sqlite3.h>
#include <signal.h>
#include <pthread.h>

#include "build.h"
#include "config.h"
#include "g7config.h"
#include "g7ctrl.h"
#include "serial.h"
#include "connwatcher.h"
#include "g7cmd.h"
#include "libxstr/xstr.h"
#include "dbcmd.h"
#include "logger.h"
#include "utils.h"
#include "g7sendcmd.h"
#include "nicks.h"

/** Defined error numbers and the corresponding text  */
struct g7error {
    /** Error code from device */
    int errcode;
    /** A human string representation of error */
    char *errmsg;
};

/** List of texts for all device errors */
static struct g7error g7error_list[] = {
    {0, "Unknown error"},
    {1, "Incorrect password"},
    {2, "Incorrect command parameters"},
    {3, "GSM SMS base phone number or GPRS Server IP adress not set"},
    {4, "Unable to detect GSM signal"},
    {5, "GSM failed"},
    {6, "Unable to establish the GPRS connection"},
    {8, "Voice busy tone"},
    {9, "Incorrect PIN code setting"},
};

/** Size of g7error_list (number of entries) */
const size_t g7error_list_num = sizeof (g7error_list) / sizeof (struct g7error);

/** Max number of outstanding command (over GPRS) to device */
#define MAX_CMDQUEUE_LEN 128

/** Queue of commands sent to the device */
struct cmdqueue_t *cmdq;

/** Current number of active commands int the command queue */
size_t cmdq_len = 0;

/** Timeout for waiting on a reply from a device over GPRS (30 sec) */
#define CMDQUEUE_TIMEOUT 30U

/**
 * Initialize command queue
 */
void
cmdqueue_init(void) {
    cmdq = _chk_calloc_exit(MAX_CMDQUEUE_LEN * sizeof (struct cmdqueue_t));
    cmdq_len = 0;
}

/**
 * Insert a command in the command queue
 * @param user_sockd User socket to write command reply to
 * @param devid Device id
 * @param tag Command tag
 * @param cmdstr The full command string
 * @return -1 on failure, >= 0 The command index where the command
 * was inserted
 * @see cmdq
 */
int
cmdqueue_insert(const int user_sockd, const unsigned devid, const char *tag, const char *cmdstr) {
    if (cmdq_len >= MAX_CMDQUEUE_LEN) {
        logmsg(LOG_ERR, "Command queue is full");
        return -1;
    }
    if (strlen(tag) > 5 || strlen(cmdstr) > 255) {
        return -1;
    }
    // Find the first empty place
    pthread_mutex_lock(&cmdqueue_mutex);
    for (size_t i = 0; i < MAX_CMDQUEUE_LEN; ++i) {
        if (0 == cmdq[i].devid && 0 == cmdq[i].ts) {
            cmdq[i].ts = time(NULL);
            cmdq[i].devid = devid;
            cmdq[i].user_sockd = user_sockd;
            xstrlcpy(cmdq[i].tag, tag, sizeof (cmdq[i].tag));
            xstrlcpy(cmdq[i].cmd, cmdstr, sizeof (cmdq[i].cmd));
            logmsg(LOG_DEBUG, "QUEUE: Inserted [%u:%s] cmd %s", devid, tag, cmdstr);
            pthread_mutex_unlock(&cmdqueue_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&cmdqueue_mutex);
    return -1;
}

/**
 * Wait until for a reply on the specified command queue index. The maximum
 * wait time is specified by CMDQUEUE_TIMEOUT define. If the waiting time is
 * exceeded we return -1
 * Regardless of whether a reply is received or not the command queue at
 * the specified index is cleared before the function returns,
 *
 * @param idx Index in command queue
 * @param replybuff Buffer to store the command reply in
 * @param maxreply Max size of reply buffer
 * @return 0 we have a reply, -1 timeout or illegal index
 * @see cmdqueue_clridx()
 */
int
cmdqueue_check_reply(const size_t idx, char *replybuff, const size_t maxreply) {
    _Bool timeout = FALSE;
    if (idx >= MAX_CMDQUEUE_LEN) {
        logmsg(LOG_ERR, "Internal error. Illegal index in cmdqueue_check_reply()");
        return -1;
    }
    const unsigned ts = cmdq[idx].ts;
    while (!timeout) {
        if (cmdq[idx].validreply) {
            xmb_strncpy(replybuff, cmdq[idx].reply, maxreply);
            cmdqueue_clridx(idx);
            return 0;
        }
        timeout = (time(NULL) - ts) > CMDQUEUE_TIMEOUT;
        usleep(100000);
    }
    cmdqueue_clridx(idx);
    return -1;
}

/*
 * Return a pointer to the specified command queue
 * @param idx Index in queue
 * @param[out] entry A pointer to a command queue entry
 * @return 0 on success, -1 if index does not exist
 */
/*
int
cmdqueue_get(size_t idx,struct cmdqueue_t **entry) {
    if( cmdq_len >= MAX_CMDQUEUE_LEN ) {
        logmsg(LOG_ERR,"Trying to read past command queue end");
        return -1;
    }
    // Sanity check
    if( 0 == cmdq[idx].devid || 0 == cmdq[idx].user_sockd) {
        logmsg(LOG_ERR,"Command queue entry seems to be empty");
        return -1;
    }
 *entry = &cmdq[idx];
    return 0;
}
 */

/**
 * Search the command queue for a matching command for the given device id
 * @param devid Device id of device we were sending this command to
 * @param tag Optional tag in command
 * @param[out] entry This is set to the found entry in queue
 * @return 0 if found, -1 if not found
 */
int
cmdqueue_match(const unsigned devid, const char *tag, struct cmdqueue_t **entry) {
    logmsg(LOG_DEBUG, "QUEUE: Searching for match: [%u:\"%s\"]", devid, tag);
    _Bool found = FALSE;
    for (size_t i = 0; !found && i < MAX_CMDQUEUE_LEN; i++) {
        logmsg(LOG_DEBUG, "QUEUE: Comparing to [%u:\"%s\"]", cmdq[i].devid, cmdq[i].tag);
        if (cmdq[i].devid && cmdq[i].devid == devid) {
            if (*tag) {
                if (0 == strcmp(tag, cmdq[i].tag)) {
                    found = TRUE;
                    *entry = &cmdq[i];
                }
            } else {
                found = TRUE;
                *entry = &cmdq[i];
            }
        }
    }
    logmsg(LOG_DEBUG, "QUEUE Match result: %d", (int) found);
    return found ? 0 : -1;
}

/**
 * Clear the specified queue position
 * @param idx Queue position
 *  @return 0 if found, -1 if not found
 */
int
cmdqueue_clridx(const size_t idx) {
    if (idx < MAX_CMDQUEUE_LEN) {
        pthread_mutex_lock(&cmdqueue_mutex);
        memset(cmdq + idx, 0, sizeof (struct cmdqueue_t));
        pthread_mutex_unlock(&cmdqueue_mutex);
        return 0;
    } else {
        return -1;
    }
}

/**
 * Return a pointer to a static string describing the device error
 * @param errcode
 * @param[out] errmsg Set to a static pointer to the errmsg
 * @return 0 on success, -1 if errcode wasn't found
 */
int
device_strerr(const int errcode, char **errmsg) {
    static char _unknown[64];
    for (size_t i = 0; i < g7error_list_num; ++i) {
        if (g7error_list[i].errcode == errcode) {
            *errmsg = g7error_list[i].errmsg;
            return 0;
        }
    }
    snprintf(_unknown, sizeof (_unknown), "Internal error: Device error code \"%d\" unknown!", errcode);
    *errmsg = _unknown;
    return -1;
}

/**
 * Return the device PIN. This is currently a dummy function that always return
 * the default PIN "0000"
 * @param maxlen Maximum length for pin buffer
 * @param[out] pinbuff Buffer to write the PIN to
 * @return 0 on success, -1 on failure
 */
int
get_device_pin(char *pinbuff, const size_t maxlen) {
    if (maxlen < 5) {
        logmsg(LOG_CRIT, "Internal error. _get_pin() maxlen too small.");
        return -1;
    }
    xmb_strncpy(pinbuff, "0000", maxlen);
    return 0;
}

/** The running tag number */
static unsigned short tagnbr = 0;

/**
 * Get the current tag to use when sending command. This is currently not
 * in use.
 * @param maxlen
 * @param[out] tagbuff
 * @return 0 on success, -1 on failure
 */
int
get_device_tag(char *tagbuff, const size_t maxlen) {
    if (maxlen < 6) {
        logmsg(LOG_CRIT, "Internal error. _get_tag() maxlen too small.");
        return -1;
    }
    pthread_mutex_lock(&cmdtag_mutex);
    tagnbr++;
    if (tagnbr > 9999)
        tagnbr = 0;
    snprintf(tagbuff, maxlen, "%04hu", tagnbr);
    pthread_mutex_unlock(&cmdtag_mutex);
    return 0;
}

/**
 * Extract the first field in the reply from the device. This means
 * reading past the "$OK" to the chars after the "=" char and returning 
 * the very first field
 * @param raw The raw answer from the device
 * @param[out] reply The extracted part of the reply
 * @param maxreply Maximum length of reply
 * @return 0 on success, -1 on failure (ERR response)
 */
int
extract_devcmd_reply_first_field(const char *raw, char *reply, size_t maxreply) {
    _Bool isok;
    char cmd[14], tag[7];
    struct splitfields flds;
    int rc = extract_devcmd_reply(raw, &isok, cmd, tag, &flds);
    if (0 == rc) {
        if (!isok)
            return -1;
        *reply = '\0';
        if (flds.nf > 0) {
            xmb_strncpy(reply, flds.fld[0], maxreply);
            if (reply[strlen(reply) - 2] == '\r' && reply[strlen(reply) - 1] == '\n')
                reply[strlen(reply) - 2] = '\0';
        }
        return 0;
    }
    return -1;
}

/**
 * Split a full device reply into its different parts
 * @param raw Raw command
 * @param[out] isok 0 if reply is "$ERR:", 1 on "$OK:". If this is an ERR reply
 * the the first parameter in the flds argument will hold the error code
 * @param[out] cmd the device command that gave this reply
 * @param[out] tag the command tag
 * @param[out] flds the different fields in the reply
 * @return 0 on success, -1 on failure in which case all out parameters are
 * undefined.
 * @see extract_devcmd_reply_simple()
 */
int
extract_devcmd_reply(const char *raw, _Bool *isok, char *cmd, char *tag, struct splitfields *flds) {
    // $ERR:<COMMAND>+[Tag]=[Error Code]
    // $OK:<COMMAND>+[Tag]=<CMDREPLY>
    const char *p = raw;
    *cmd = '\0';
    *tag = '\0';
    flds->nf = 0;

    if ('$' == *p) {

        p++;
        if ('O' == *p && 'K' == *(p + 1) && ':' == *(p + 2)) {
            *isok = TRUE;
            p += 3;
        } else if ('E' == *p && 'R' == *(p + 1) && 'R' == *(p + 2) && ':' == *(p + 3)) {
            *isok = FALSE;
            p += 4;
            // Some commands just return "$ERR:n" where n is the error code. Check for this case
            if (*p >= '0' && *p <= '9') {
                flds->nf = 1;
                flds->fld[0][0] = *p;
                flds->fld[0][1] = '\0';
                return 0;
            }

        } else {
            return -1;
        }

        // Find command name
        const char *pp = p;
        size_t max = 14;
        while (max-- > 0 && *pp != '+' && *pp != '=')
            ++pp;
        if (0 == max)
            return -1;

        // Copy command to out parameter
        char *cp = cmd;
        while (p < pp) {
            *cp++ = *p++;
        }
        *cp = '\0';
        *tag = '\0';
        if (*p == '+' && ! *cmd) {
            return -1;
        }

        // Check for optional tag
        if (*p == '+') {
            p++;
            pp = p;
            max = 7;
            while (max-- > 0 && *pp != '=' && *pp != '\r' && *pp != '\n') {
                ++pp;
            }
            if (0 == max)
                return -1;
            while (p < pp) {
                *tag++ = *p++;
            }
            *tag = '\0';
        }

        // p now either points to the end of the string for replies such as "$OK:RESET+0009"
        // or to a '=' when the command reply with data. For that case we extract all the data.
        if (*p && *p != '\r' && *p != '\n') {
            p++;
            if (xstrsplitfields(p, ',', flds)) {
                return -1;
            }
        } else {
            flds->nf = 0;
        }

        // An error response have one parameter (the error code) so make
        // sanity check
        if (!isok && flds->nf != 1)
            return -1;


    } else {

        return -1;

    }

    return 0;
}

/**
 * Handle a reply from device and give it back to the user with
 * error checking.
 * @param sockd Client socket
 * @param reply Raw reply from device to be parsed
 * @param tagbuff The expected tag in the reply
 * @return -1 if device did not return a complete reply,0 if success
 * If return code is between [-10, -2] device this indicates device error
 * number.
 */
#define RESP_LEN 128
int
handle_device_reply(const int sockd, const char *reply, const char *tagbuff) {
    char *rbuff = strdup(reply);

    if ('$' != *reply && '3' == *reply && strlen(reply) > 60) {
        // 3000000001,20140119234939,17.960205,59.366856,0,0,0,0,1,4.20V,0
        logmsg(LOG_INFO, "Device location event: %s", rbuff);
        _writef(sockd, "%s%s", DEVICE_REPLY_PROMPT, rbuff);
        free(rbuff);
        return 0;
    }

    _Bool isok;
    char cmdname[16];
    char tag[7];
    struct splitfields flds;
    int rc = extract_devcmd_reply(reply, &isok, cmdname, tag, &flds);
    if (rc) {
        _writef(sockd, "[ERR] Incomplete reply from device \"%s\"", reply);
        logmsg(LOG_ERR, "Incomplete reply from device: \"%s\" ", reply);
    } else {

        if (strcmp(tag, tagbuff)) {
            logmsg(LOG_ERR, "Expected tag=\"%s\" in reply but found \"%s\". Trying to flush serial buffer", tagbuff, tag);
            _writef(sockd, "[ERR] Unexpected reply from device \"%s\"", reply);
            free(rbuff);
            return -1;
        }
        if (!isok) {
            // Device responded with an error code
            int errcode = xatoi(flds.fld[0]);
            char *errstr;
            device_strerr(errcode, &errstr);
            logmsg(LOG_ERR, "Device error: (%d : %s)", errcode, errstr);
            _writef(sockd, "[ERR] ( %d : %s )", errcode, errstr);
            rc = -(errcode + 1);
        } else {
            // Device normal response
            char resp[RESP_LEN];
            if (flds.nf > 0) {
                xstrlcpy(resp, flds.fld[0], sizeof (resp));
                for (size_t i = 1; i < flds.nf; i++) {
                    xstrlcat(resp, ",", sizeof (resp));
                    xstrlcat(resp, flds.fld[i], sizeof (resp));
                }
            } else {
                xstrlcpy(resp, "OK", sizeof (resp));
            }
            _writef(sockd, "%s%s", DEVICE_REPLY_PROMPT, resp);
            if (flds.nf > 0 && translateDeviceReply) {
                if (cmdarg_to_text(sockd, cmdname, &flds)) {
                    _writef(sockd, "\n[ERR] Cannot parse device reply");
                }
            }

        }
    }
    free(rbuff);
    return rc;
}

/** Holds the socket to the connected device we want to talk to over GPRS on */
//static ssize_t device_target_socket = -1;
//static unsigned device_target_devid = 0;

/**
 * Set the target device that we are going to send the commands to. The
 * client_nbr refers to the order number of the connected client
 * @param[in] cli_info Client info structure that holds information about the current
 *                 command client that is connecting to us in this thread. Among
 *                 other things this structure stores the socket used to communicate back
 *                 to the client as well as information about which device the client
 *                 has set as target device.
 * @param[in] client_nbr The order number of the client as liste by the "lc"
 * command. If set to "-1" then the communication is reset to USB.
 * @return 0 on success, -1 on failure
 * @see set_gprs_device_target_by_nickname()
 */
int
set_gprs_device_target_by_index(struct client_info *cli_info, const ssize_t client_nbr) {

    const int sockd = cli_info->cli_socket;

    if (client_nbr == -1) {
        // Set back to USB communication
        logmsg(LOG_DEBUG, "Target reset to USB");
        _writef(sockd, "Target reset to USB.");
        cli_info->target_socket = -1;
        cli_info->target_deviceid = 0;
        cli_info->target_cli_idx = -1;
        return 0;
    }

    logmsg(LOG_DEBUG, "Trying to find device target: %zd", client_nbr);
    // Find the client_nbr:th connected client that is a device
    //
    ssize_t nbr = 0;
    size_t idx = 0;
    for (idx = 0; idx < max_clients; ++idx) {
        if (client_info_list[idx].cli_thread && !client_info_list[idx].cli_is_cmdconn) {
            nbr++;
            if (nbr == client_nbr) {
                // Only device target can be given
                //if (client_info_list[idx].cli_is_cmdconn) {
                //    logmsg(LOG_ERR, "Client number %zd must be a device. Not a command client", client_nbr);
                //    _writef(sockd, "[ERR] Client %zd is not tracker.", nbr);
                //    return -1;
                // }
                break;
            }
        }
    }

    if (idx >= max_clients) {
        logmsg(LOG_ERR, "Device number %zd does not exist", client_nbr);
        _writef(sockd, "[ERR] Device number %zd does not exist.", client_nbr);
        return -1;
    }

    cli_info->target_socket = client_info_list[idx].cli_socket;
    cli_info->target_deviceid = client_info_list[idx].cli_devid;
    cli_info->target_cli_idx = idx;
    logmsg(LOG_DEBUG, "Device target info: %02zd-> %u at %s:%d",
            client_nbr,
            client_info_list[idx].cli_devid,
            client_info_list[idx].cli_ipadr, client_info_list[idx].cli_socket);
    if (0 == client_info_list[idx].cli_devid) {
        _writef(sockd, "Target set to client %zd. Device ID not yet known.", client_nbr);
        logmsg(LOG_DEBUG, "Target set to client %zd. Device ID not yet known.", client_nbr);
    } else {
        _writef(sockd, "Target set to client %zd. Device id: [%u]", client_nbr, client_info_list[idx].cli_devid);
        logmsg(LOG_DEBUG, "Target set to client %zd. Device id: [%u]", client_nbr, client_info_list[idx].cli_devid);
    }
    return 0;
}

/**
 * Set the target device that we are going to send the commands to by its nickname.
 * We use the nickname to find the corresponding device ID and then use this information
 * to find it in the list of connected clients.
 * @param nickname Nick name of device to use as target
 * @param sockd Client socket to communicate on
 * @return 0 on success, -1 on failure
 * @see set_gprs_device_target_by_index()
 */
int
set_gprs_device_target_by_nickname(struct client_info *cli_info, const char *nickname) {

    const int sockd = cli_info->cli_socket;
    char devid[11];
    if (db_get_devid_from_nick(nickname, devid)) {
        logmsg(LOG_ERR, "Cannot get devid from nickname");
        _writef(sockd, "[ERR] Nick name \"%s\" does not exist", nickname);
        return -1;
    }
    unsigned di = xatol(devid);

    size_t idx = 0;
    for (idx = 0; idx < max_clients; ++idx) {
        if (di == client_info_list[idx].cli_devid) {
            break;
        }
    }

    if (idx >= max_clients) {
        logmsg(LOG_ERR, "Device with ID %s is not connected", devid);
        _writef(sockd, "[ERR] Device with nick %s (ID=%s) is not yet connected", nickname, devid);
        return -1;
    }

    cli_info->target_socket = client_info_list[idx].cli_socket;
    cli_info->target_deviceid = di;

    _writef(sockd, "Target set to tracker with device id: %u (%s)", di, nickname);

    return 0;
}

/**
 * Sends the specified command to the device over GPRS
 * The device is connected on the specified socket which we use to
 * send the command over. Since the reply also comes over a socket at
 * a later time we insert the ID of this command in the cmd queue
 * which is used in the tracker thread to match a reply from the tracker
 * with this command once it comes back.
 * @param[in] cli_info Client info structure that holds information about the current
 *                 command client that is connecting to us in this thread. Among
 *                 other things this structure stores the socket used to communicate bac
 *                 to the client as well as information about which device the client
 *                 has set as target device.
 * @param[in] cmd Command string to send to the device (including "\r\n")
 * @param[in] tagbuff The tag-id used in the command
 * @param[out] replybuff Reply from device, If replybuff is NULL then ignore
 * the reply read back from the device and just return
 * @param[in] maxreply Maximum size of replybuff
 * @return -1 on failure, 0 on success
 */
int
send_rawcmd_reply_over_gprs(struct client_info *cli_info, const char *cmd, const char *tagbuff, char *replybuff, size_t maxreply) {

    const int sockd = cli_info->cli_socket;

    char cmdbuff[1024];
    snprintf(cmdbuff, sizeof (cmdbuff), "%s\r\n", cmd);

    // First check if this client is still connected
    _Bool found = FALSE;
    for (size_t i = 0; i < max_clients && !found; i++) {
        found = (client_info_list[i].cli_devid == cli_info->target_deviceid &&
                client_info_list[i].cli_socket == cli_info->target_socket);
    }

    if (!found) {
        logmsg(LOG_ERR, "Device is no longer connected (%u). Resetting target device to USB", cli_info->target_deviceid);
        _writef(sockd, "[ERR] Device is no longer connected.");
        set_gprs_device_target_by_index(cli_info, -1);
        return -1;
    }

    // Insert this command in the device queue so that the tracker
    // thread can match this command when the reply comes back
    int cmdqueue_idx = cmdqueue_insert(sockd, cli_info->target_deviceid, tagbuff, cmd);
    if (cmdqueue_idx < 0) {
        logmsg(LOG_ERR, "Cannot insert command in queue");
        return -1;
    }

    int rc = write(cli_info->target_socket, cmdbuff, strlen(cmdbuff));
    if (rc != (int) strlen(cmdbuff)) {
        logmsg(LOG_ERR, "Failed to send command to device");
        _writef(sockd, "[ERR] Could not write to device.");
        return -1;
    }

    // Now wait until we get a reply in the tracker thread or until
    // we get a timeout
    char reply[1024];
    if (cmdqueue_check_reply(cmdqueue_idx, reply, sizeof (reply))) {
        // We got a timeout
        logmsg(LOG_ERR, "No reply from device");
        _writef(sockd, "[ERR] No reply from device.");
        return -1;
    }
    logmsg(LOG_DEBUG, "Handling reply (%s)", reply);


    // Drop ending "\r\n"
    size_t _len = strlen(reply);
    if (reply[_len - 2] != '\r' || reply[_len - 1] != '\n') {
        logmsg(LOG_DEBUG, "Was expecting device reply to end with \"\\r\\n\"");
        _writef(sockd, "[ERR] Unknown device reply.");
        return -1;
    }
    reply[_len - 2] = '\0';


    rc = handle_device_reply(sockd, reply, tagbuff);
    if (0 == rc) {
        if (replybuff != NULL && maxreply > 0) {
            snprintf(replybuff, maxreply, "%s", reply);
        }
    }

    return rc;
}

/**
 * Sends the specified command to the device over the virtual serial port and wait for
 * reply.
 *
 * @param[in] cli_info Client info structure that holds information about the current
 *                 command client that is connecting to us in this thread. Among
 *                 other things this structure stores the socket used to communicate bac
 *                 to the client as well as information about which device the client
 *                 has set as target device.
 * @param[in] cmd Command string to send to the device (including "\r\n")
 * @param[out] replybuff Reply from device, If replybuff is NULL then ignore
 * the reply read back from the device and just return
 * @param[in] maxreply Maximum size of replybuff
 * @return -1 on failure, 0 on success
 */
int
send_rawcmd_reply_over_usb(struct client_info *cli_info, const char *cmd, const char *tagbuff, char *replybuff, size_t maxreply) {

    const int sockd = cli_info->cli_socket;

    char cmdbuff[1024];
    snprintf(cmdbuff, sizeof (cmdbuff), "%s\r\n", cmd);

    // Verify that the device is really connected when a command is executed by a 
    // client. For the case the daemon during connection phase tries to send a command
    // to see if a device is present we ignore the check (egg or chicken problem). 
    if (cli_info->cli_socket >= 0 && !is_usb_connected(cli_info->target_usb_idx)) {
        logmsg(LOG_DEBUG, "Device not connected reply from is_usb_connected()");
        _writef(sockd, "[ERR] Command not possible. Device not connected.");
        return -1;
    }

    //const char *device = get_stty_device_name(cli_info->target_usb_idx);
    char *device;
    if (-1 == get_usb_devicename(cli_info->target_usb_idx, &device)) {
        logmsg(LOG_ERR, "Cannot get USB device name for index=%zd", cli_info->target_usb_idx);
        return -1;
    }

    logmsg(LOG_DEBUG, "Sending device command : \"%s\" to device \"%s\"", cmd, device);

    int sfd = serial_open(device, DEVICE_BAUD_RATE);
    if (sfd < 0) {
        _writef(sockd, "[ERR] Failed to open communication with USB device with index = %d", (int) cli_info->target_usb_idx);
        return -1;
    }

    int rc = serial_write(sfd, cmdbuff, strlen(cmdbuff));

    if (-1 == rc) {
        _writef(sockd, "[ERR] Failed to write command to device");
        logmsg(LOG_ERR, "Cannot write command to device");
        serial_close(sfd);
        return -1;
    }

    char *reply = NULL;
    rc = serial_read_line(sfd, &reply);

    serial_close(sfd);

    if (-1 == rc || reply == NULL) {
        _writef(sockd, "[ERR] Failed to read reply from device.");
        return -1;
    }

    // Drop ending "\r\n"
    const size_t _len = strlen(reply);
    if (_len <= 2) {
        logmsg(LOG_ERR, "Invalid reply from device, less than 2 characters was sent back");
        _writef(sockd, "[ERR] Unknown device reply.");
        free(reply);
        return -1;
    }

    if (reply[_len - 2] != '\r' || reply[_len - 1] != '\n') {
        logmsg(LOG_ERR, "Was expecting device reply to end with \"\\r\\n\"");
        _writef(sockd, "[ERR] Unknown device reply.");
        free(reply);
        return -1;
    }
    reply[_len - 2] = '\0';

    rc = handle_device_reply(sockd, reply, tagbuff);
    if (0 == rc) {
        if (replybuff != NULL && maxreply > 0) {
            snprintf(replybuff, maxreply, "%s", reply);
        }
    }

    free(reply);
    return rc;
}

/**
 * Sends the specified command to the device.
 * Communication back to the user is done over the  specified socket. The
 * function will store the reply back from the device in the replybuff.
 * If replybuff is NULL then the reply is read from the device but
 * ignored and the function just returns after checking if the reply
 * indicates an error or success.
 * This is the single point of contact between the daemon and the device
 * since ALL communication to/from device will be going through here.
 * @param[in] cli_info Client info structure that holds information about the current
 *                 command client that is connecting to us in this thread. Among
 *                 other things this structure stores the socket used to communicate bac
 *                 to the client as well as information about which device the client
 *                 has set as target device.
 * @param[in] cmd Command string to send to the device (including "\r\n")
 * @param[in] tagbuff The tag-id used in the command
 * @param[out] replybuff Reply from device, If replybuff is NULL then ignore
 * the reply read back from the device and just return
 * @param[in] maxreply Maximum size of replybuff
 * @return -1 on failure, 0 on success
 */
int
send_rawcmd_reply(struct client_info *cli_info, const char *cmd, const char *tagbuff, char *replybuff, size_t maxreply) {

    // There are two ways we can talk to the device, either over GPRS
    // or directly over USB (virtual serial port). If the device_target_socket
    // is valid this means that we have a GPRS connection that we talk over.
    if (cli_info->target_socket > 0 && cli_info->target_cli_idx >= 0) {
        logmsg(LOG_DEBUG, "Calling send_rawcmd_reply_over_gprs()");
        return send_rawcmd_reply_over_gprs(cli_info, cmd, tagbuff, replybuff, maxreply);
    } else if (cli_info->target_usb_idx >= 0) {
        logmsg(LOG_DEBUG, "Calling send_rawcmd_reply_over_usb()");
        return send_rawcmd_reply_over_usb(cli_info, cmd, tagbuff, replybuff, maxreply);
    } else {
        logmsg(LOG_ERR, "Fatal internal error: The device does not exist in send_rawcmd_reply()");
        return -1;
    }
}

/**
 * Send a raw command to the device without waiting for a reply
 * @param sockd
 * @param cmd
 * @param tagbuff
 * @return .1 on failure , 0 on success
 */
int
send_rawcmd(struct client_info *cli_info, const char *cmd, const char *tagbuff) {
    return send_rawcmd_reply(cli_info, cmd, tagbuff, NULL, 0);
}

/**
 * Helper function to send a user device command to the actual device.
 * The function translates the user command to the raw command, sends it
 * to the device and reads the reply.
 * @param[in] cli_info Client info structure that holds information about the current
 *                 command client that is connecting to us in this thread. Among
 *                 other things this structure stores the socket used to communicate bac
 *                 to the client as well as information about which device the client
 *                 has set as target device.
 * @param cmd User command
 * @param reply Reply from device
 * @param maxreply Maximum length of reply buffer
 * @return  0 on success, -1 on failure
 */
int
send_cmdquery_reply(struct client_info *cli_info, const char *cmd, char *reply, const size_t maxreply) {
    char pinbuff[16];
    char tagbuff[16];

    if (-1 == get_device_pin(pinbuff, sizeof (pinbuff)) ||
            -1 == get_device_tag(tagbuff, sizeof (tagbuff))) {
        return -1;
    }
    char rawcmd[16];
    if (-1 == get_devcmd_from_srvcmd(cmd, sizeof (rawcmd), rawcmd)) {
        logmsg(LOG_ERR, "_dev_cmd_help(): Cannot find user command %s", cmd);
        return -1;
    }
    char devcmd[128];

    // For the special case of the SETEVT command we also need to specify a event id in the GET command
    // so we use the "reply" argument as input in this special case. This is kludge!!
    if (0 == strcmp("SETEVT", rawcmd)) {
        snprintf(devcmd, sizeof (devcmd), "$WP+%s+%s=%s,%s,?", rawcmd, tagbuff, pinbuff, reply);
        logmsg(LOG_DEBUG, "Handle GFEVT in send_cmdquery_reply() %s", devcmd);
    } else {
        snprintf(devcmd, sizeof (devcmd), "$WP+%s+%s=%s,?", rawcmd, tagbuff, pinbuff);
    }

    // We don't want any output to the user at this level so set sockd to -1 temporarily
    const int old_sockd = cli_info->cli_socket;
    cli_info->cli_socket = -1;
    logmsg(LOG_DEBUG, "Prepared raw command \"%s\" for USB idx=%zd", devcmd, cli_info->target_usb_idx);
    *reply = '\0';
    int rc = send_rawcmd_reply(cli_info, devcmd, tagbuff, reply, maxreply);
    cli_info->cli_socket = old_sockd;

    if (-1 == rc) {
        logmsg(LOG_ERR, "_dev_cmd_help : Failed command \"%s\"", cmd);
        return -1;
    }
    return 0;
}

/**
 * Read number of stored locations in memory at device
 * @param[in] cli_info Client info structure that holds information about the current
 *                 command client that is connecting to us in this thread. Among
 *                 other things this structure stores the socket used to communicate bac
 *                 to the client as well as information about which device the client
 *                 has set as target device.
 * @param[out] numrec Number of stored location in device memory
 * @return 0 on success, -1 on failure
 */
int
get_numrec(struct client_info *cli_info, int *numrec) {
    *numrec = 0;
    char dlreply[128];
    if (-1 == send_cmdquery_reply(cli_info, "DLREC", dlreply, sizeof (dlreply))) {
        return -1;
    }
    // The reply has the format
    // $OK:DLREC[+TAG]=17254(20140107231903-20140109231710)
    if (strncmp(dlreply, "$OK:", 4)) {
        logmsg(LOG_ERR, "Cannot read number of locations stored in memory (%s)", dlreply);
        return -1;
    }
    char *dlptr = dlreply; // Second digit (there is always one digit))
    // The number of records always comes after the '=' sign
    while (*dlptr && *dlptr != '=')
        dlptr++;
    if (! *dlptr) {
        logmsg(LOG_ERR, "Invalid response from DLREC: %s", dlreply);
        return -1;
    }
    dlptr++;
    int dcnt = 0;
    char digits[7];
    while (dcnt < 6 && *dlptr != '(') {
        digits[dcnt++] = *dlptr++;
    }
    if (dcnt >= 6) {
        logmsg(LOG_ERR, "Invalid response from DLREC: %s", dlreply);
        return -1;
    }
    digits[dcnt] = '\0';
    *numrec = xatoi(digits);
    return 0;
}

/**
 * Read all internally recorded locations stored in the device internal
 * memory and store them in the database. After a successful read the
 * device internal memory is cleared.
 * @param sockd Socket used for user communication
 * @return 0 on success, -1 on failure
 */
int
get_dlrec_to_db(struct client_info *cli_info) {

    const int sockd = cli_info->cli_socket;

    time_t t1 = time(NULL);

    _writef(sockd, "Preparing, please wait ...\n");

    // First find the number of locations stored in device (to be able to
    // give user progress information during the download)
    int num_loc;
    if (-1 == get_numrec(cli_info, &num_loc)) {
        logmsg(LOG_ERR, "Failed to retrieve number of locations in device memory");
        _writef(sockd, "[ERR] Failed to initialize download.");
        return -1;
    }
    logmsg(LOG_DEBUG, "Number of locations in device memory: %d", num_loc);

    // Get a buffer big enough to hold all locations
    size_t max_read = 1024 + num_loc * 70;
    _Bool finished = FALSE;

    char *rbuff = _chk_calloc_exit(max_read);
    char *bptr = rbuff;
    size_t cnt = 0;
    _writef(sockd, "Reading %d locations from device, please wait ...\n", num_loc);

    // We want a mark at each 10:th (i.e. 10%) of the total number of locations
    // we download. In case there are fewer than 10 locations in total we put a
    // mark at every locations.
    int progress_mark = num_loc > 10 ? num_loc / 10 : 1;
    // Setup for the progress marks.
    int progress_nread = progress_mark * 66; // To print a progress mark at first read
    int progress_cnt = 0;
    int nread;

    // Verify that the device is really connected
    if (!is_usb_connected(cli_info->target_usb_idx)) {
        _writef(sockd, "[ERR] Command not possible. Device not connected.");
        free(rbuff);
        return -1;
    }

    char *device;
    if (-1 == get_usb_devicename(cli_info->target_usb_idx, &device)) {
        logmsg(LOG_ERR, "Cannot get USB device name for index=%zd", cli_info->target_usb_idx);
        return -1;
    }
    int sfd = serial_open(device, DEVICE_BAUD_RATE);
    if (sfd < 0) {
        free(rbuff);
        return -1;
    }

    char pinbuff[16];
    char tagbuff[16];
    if (-1 == get_device_pin(pinbuff, sizeof (pinbuff)) ||
            -1 == get_device_tag(tagbuff, sizeof (tagbuff))) {
        free(rbuff);
        return -1;
    }
    char cmdbuff[64];
    snprintf(cmdbuff, sizeof (cmdbuff), "$WP+DLREC+%s=%s,0,0\r\n", tagbuff, pinbuff);
    int rc = serial_write(sfd, cmdbuff, strlen(cmdbuff));

    if (-1 == rc) {
        _writef(sockd, "[ERR] Cannot write command to device.");
        logmsg(LOG_ERR, "Cannot write command to device");
        serial_close(sfd);
        free(rbuff);
        return -1;
    }

    do {

        nread = serial_read_timeout(sfd, max_read, bptr, 5000); // 5s timeout

        if (nread < 0) {
            logmsg(LOG_ERR, "Error while reading recorded positions");
            _writef(sockd, "\n[ERR] Error while reading from device.");
            serial_close(sfd);
            free(rbuff);
            return -1;
        }
        *(bptr + nread) = '\0';

        const size_t len = strlen(bptr);
        static const char *END_STR = "$MSG:Download Completed\r\n";
        const size_t elen = strlen(END_STR);
        // Check for end indicated by "$Download Completed" string as the
        if (len >= elen && 0 == xstricmp(bptr + len - elen, END_STR)) {
            logmsg(LOG_DEBUG, "Got \"Download Completed\" message from device");
            finished = TRUE;
            nread -= elen; // We don't want the end mark to be included
        }
        bptr += nread;
        max_read -= nread;
        progress_nread += nread;

        // Each location string is approx 66 chars long so if we want a mark
        // for every 1000 locations then we need to count up to 660000
        if (progress_nread > progress_mark * 66) {
            _writef(sockd, "[%d%%].", progress_cnt);
            logmsg(LOG_DEBUG, "Downloaded ~%d%%", progress_cnt);
            progress_cnt += 10;
            progress_nread = 0;
        }
        cnt++;

    } while (!finished && max_read > 0);

    _writef(sockd, "[100%%]\n");

    snprintf(cmdbuff, sizeof (cmdbuff), "$WP+SPDLREC+%s=%s\r\n", tagbuff, pinbuff);
    rc = serial_write(sfd, cmdbuff, strlen(cmdbuff));

    serial_close(sfd);

    if (-1 == rc) {
        logmsg(LOG_ERR, "Failed to write stop download command to device");
    }

    if (max_read <= 0) {
        _writef(sockd, "[ERR] Internal error. Buffer to read data from device too small");
        logmsg(LOG_ERR, "DLREC : Buffer to read data from device too small!");
        free(rbuff);
        return -1;
    }

    *bptr = '\0';
    _writef(sockd, "Storing a copy of read locations in \"%s\" ...\n", LAST_DLREC_FILE);

    (void) _wbuff2file(LAST_DLREC_FILE, rbuff, strlen(rbuff));

    // Check that the first line is $OK:DLREC+<TAG>=0,0
    bptr = rbuff;
    if (*bptr != '$' || *(bptr + 1) != 'O' || *(bptr + 2) != 'K') {
        free(rbuff);
        return -1;
    }
    while (*bptr != '\r')
        bptr++;
    bptr += 2;
    // Verify that this points to the first character of the first device id
    if (*bptr != '3' && *bptr != '[') {
        free(rbuff);
        _writef(sockd, "[ERR] Corrupt data read from device.");
        return -1;
    }

    const time_t t2 = time(NULL);

    if (0 == strlen(bptr)) {
        _writef(sockd, "[ERR] No locations in memory.\n");
    } else {
        if (use_address_lookup)
            _writef(sockd, "Updating DB. Address lookup used  ...\n");
        else
            _writef(sockd, "Updating DB ...\n");
        int num = db_store_locations(sockd, num_loc, bptr, NULL, NULL);
        if (num > 0) {
            _writef(sockd, "Checking for duplicates ...\n");
            // Now remove any duplicates
            char *sql_dedup = "DELETE FROM tbl_track "
                    "WHERE fld_key NOT IN "
                    "("
                    "SELECT MIN(fld_key) "
                    "FROM tbl_track "
                    "GROUP BY fld_deviceid,fld_datetime "
                    ");VACUUM;";
            int changes;
            rc = db_exec_sql(sql_dedup, &changes);
            if (0 == rc) {
                if (changes > 0) {
                    _writef(sockd, "%d locations imported, %d duplicates removed (already imported).\n", num, changes);
                } else {
                    _writef(sockd, "%d locations imported.\n", num);
                }
                const time_t t3 = time(NULL);
                unsigned t_tot = t3 - t1;
                unsigned t_dev = t2 - t1;
                unsigned t_db = t3 - t2;

                if (t_tot > 120) {
                    unsigned t_tot_min = t_tot / 60;
                    unsigned t_tot_s = t_tot % 60;
                    unsigned t_dev_min = t_dev / 60;
                    unsigned t_dev_s = t_dev % 60;
                    unsigned t_db_min = t_db / 60;
                    unsigned t_db_s = t_db % 60;
                    _writef(sockd, "Total time: %u:%02u min (Device read: %u:%02u min, DB Update: %u:%02u min)\n", t_tot_min, t_tot_s, t_dev_min, t_dev_s, t_db_min, t_db_s);
                } else {
                    _writef(sockd, "Total time: %us (Device read: %us, DB Update: %us)\n", t_tot, t_dev, t_db);
                }

            } else {
                _writef(sockd, "[ERR] Failed to update DB. See log form more information.");
            }
        } else {
            _writef(sockd, "[ERR] Error storing location update in DB\n");
        }
    }
    free(rbuff);
    return 0;
}

/**
 * Return the device id from the device pointed to by the client_info structure
 * This will send the CONFIG raw command to the device and only extract the device is
 * @param[in] cli_info Client context
 * @param[out] device_id The returned device ID as a long integer
 * @return 0 on success, -1 on failure
 */
int
get_devid(struct client_info *cli_info, unsigned *device_id) {
    char _config[48], devid[32];
    if (-1 == send_cmdquery_reply(cli_info, "CONFIG", _config, sizeof (_config))) {
        logmsg(LOG_DEBUG, "Failed send_cmdquery_reply()");
        return -1;
    }
    if (-1 == extract_devcmd_reply_first_field(_config, devid, sizeof (devid))) {
        logmsg(LOG_DEBUG, "Failed extract_devcmd_reply_simple()");
        return -1;
    }
    *device_id = xatol(devid);
    return 0;
}

/* EOF */
