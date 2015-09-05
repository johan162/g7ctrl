/* =========================================================================
 * File:        g7sendcmd.h
 * Description: Low level routinces to send command to device
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: g7sendcmd.h 1050 2015-09-05 21:13:43Z ljp $
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

#ifndef G7SENDCMD_H
#define	G7SENDCMD_H

#ifdef	__cplusplus
extern "C" {
#endif

/** Prefix for all non-error command replies to the user */
#define DEVICE_REPLY_PROMPT "[GM7] "

/** An entry in the queue of commands sent to devices over GPRS */
struct cmdqueue_t {
    /** Socket to communicate back to user on */
    int user_sockd;
    /** Device id we are sending command to */
    unsigned devid;
    /** Timestamp when we initiated the command */
    time_t ts;
    /** Optional tag used in command */
    char tag[6];
    /** Original command string */
    char cmd[256];
    /** Reply from device */
    char reply[1024];
    /** Indicate that we have a valid reply */
    volatile _Bool validreply;
};

void
cmdqueue_init(void);

int
cmdqueue_insert(const int user_sockd, const unsigned devid, const char *tag, const char *cmdstr);

//int
//cmdqueue_get(size_t idx,struct cmdqueue_t **entry);

int
cmdqueue_match(const unsigned devid, const char *tag, struct cmdqueue_t **entry);

int
cmdqueue_clridx(const size_t idx);

int
get_device_pin(char *pinbuff, const size_t maxlen );

int
get_device_tag(char *tagbuff, const size_t maxlen);

int
set_gprs_device_target_by_index(struct client_info *cli_info, const ssize_t client_nbr);

int
set_gprs_device_target_by_nickname(struct client_info *cli_info, const char *nickname);

int
send_rawcmd_reply(struct client_info *cli_info, const char *cmd, const char *tagbuff, char *replybuff, size_t maxreply);

int
send_rawcmd(struct client_info *cli_info, const char *cmd, const char *tagbuff);

int
send_cmdquery_reply(struct client_info *cli_info, const char *cmd,char *reply, const size_t maxreply);

int
extract_devcmd_reply_first_field(const char *raw,char *reply,size_t maxreply);

int
extract_devcmd_reply(const char *raw, _Bool *isok, char *cmd, char *tag, struct splitfields *flds);

int
handle_device_reply(const int sockd, const char *rbuff, const char *tagbuff);

int
get_dlrec_to_db(struct client_info *cli_info);

int
get_numrec(struct client_info *cli_info, int *numrec);

int
get_devid(struct client_info *cli_info, unsigned *device_id);

int
device_strerr(const int errcode, char **errmsg);


#ifdef	__cplusplus
}
#endif

#endif	/* G7SENDCMD_H */

