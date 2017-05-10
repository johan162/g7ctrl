/*
 * File:   gen_cmd_xml.c
 * Author: ljp
 *
 * Created on December 15, 2013, 8:27 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*
 * Default serial port setting
 */
#define DEVICE_BAUD_RATE 57600

/*
 * Command modes for device command. All commands support the
 * writing but not all commands support the query (R) mode.
 */
#define CMD_MODE_R 1
#define CMD_MODE_W 2
#define CMD_MODE_RW (CMD_MODE_R|CMD_MODE_W)

/*
 * Type of commands
 */
#define CMD_TYPE_GET 1
#define CMD_TYPE_SET 2
#define CMD_TYPE_GETSET_BINARY 4
#define CMD_TYPE_GETSET 8
#define CMD_TYPE_DO 16

/*
 * Each command may have a number of arguments of different types. The types
 * can be either string, bool, int, float or selection from a list. These constants
 * helps define each command.
 */
#define ARGT_BOOL 0
#define ARGT_INT 1
#define ARGT_FLOAT 2
#define ARGT_SELECT 3
#define ARGT_STRING 4

// Maximum number of items in the select list
#define MAX_SELECT 15

// Maximum number of argument for any command
#define MAX_ARGS 15

// A special label for argument that is not shown to the user. Instad this argument
// is automatically set to on/off depending on what the user have specified with the
// on/off argument.
#define AUTO_ENABLE_LABEL "_Enabled"

// Internal error code. This is used to close down the thread once the user have
// disconnected.
#define USER_DISCONNECT (-99)

// Each command handler takes a socket, command index and a mode specific optional flag
typedef int (*ptrcmd)(const int, const int, const int);

// Forward prototypes for command handlers to deal with the special case
// when we download the log from the device

/*
 * The following are prototypes for the four classes of command handlers that
 * we use. One command handler caters for multiple device commands depending on
 * usage.
 * - Binary commands are commands that the user can enable/disable
 * - Get command are commands where only a read from the device is available
 * - Do commands are commands without arguments but with side effects such as
 *   resetting the device.
 * - Set command is for specifying arguments to a command.
 *
 * Note: A number of commands support both Get/Set calling. In the command list
 * the actual supported modes for a command is specified with the CMD_MODE_xxx flag.
 */
int cmd_binary_template(const int sockd, const int cmdidx, const int gsflag);
int cmd_get_template(const int sockd, const int cmdidx, const int gsflag);
int cmd_do_template(const int sockd, const int cmdidx, const int gsflag);
int cmd_set_template(const int sockd, const int cmdidx, const int gsflag);
int send_cmd_to_device(int sockd, const char *cmd);
int cmd_dlrec(const int sockd, const int cmdidx, const int gsflag);

struct g7command {
    const char *srvcmd;               // Command used in server
    const char *cmdstr;               // Command string without $WP+ prefix
    const ptrcmd cmdhandler;          // Pointer to function handling this command
    const int type;                   // Supported command types: 0=get, 1=set, 3=get/set binary, 4=get/set
    const int modes;                  // Combination of CMD_MODE_xxx flags
    const char *descrshort;           // Short description
    const char *descrlong;            // Long description
};

// TODO: get geofevt must be tweaked to take an argument !

static struct g7command g7command_list[] =  {
    /* Binary commands, i.e. on/off/read */
    { "roam",   "ROAMING",      cmd_binary_template,    CMD_TYPE_GETSET_BINARY , CMD_MODE_RW,    "Enable/Disable GPRS roaming function", "" },
    { "phone",  "VLOCATION",    cmd_binary_template,    CMD_TYPE_GETSET_BINARY , CMD_MODE_RW,    "Enable the function \"Get the current location by making a phone call\"", "" },
    { "mswitch", "SETRA",       cmd_binary_template,    CMD_TYPE_GETSET_BINARY , CMD_MODE_RW,    "Enable/Disable detached report", "" },
    { "led",    "ELED",         cmd_binary_template,    CMD_TYPE_GETSET_BINARY , CMD_MODE_RW,    "Enable/Disable the LED indicator on/off", "" },
    { "gfen",   "GFEN",         cmd_binary_template,    CMD_TYPE_GETSET_BINARY , CMD_MODE_RW,    "Enable/Disabling GEO-fencing functionality", "" },
    { "sleep",  "SLEEP",        cmd_binary_template,    CMD_TYPE_GETSET_BINARY , CMD_MODE_RW,    "Enable/Disable \"Sleeping Report\"", ""},
    { "track",  "TRACK",        cmd_binary_template,    CMD_TYPE_GETSET_BINARY , CMD_MODE_RW,    "Control if device should be sending back automatic tracking information.", ""},

    /* Get commands */
    { "loc",   "GETLOCATION",   cmd_get_template,       CMD_TYPE_GET, CMD_MODE_R,     "Get latest location", "Get latest location"},
    { "imei",   "IMEI",         cmd_get_template,       CMD_TYPE_GET, CMD_MODE_R,     "Query the IMEI number of the internal GSM module", "" },
    { "sim",    "SIMID",        cmd_get_template,       CMD_TYPE_GET, CMD_MODE_R,     "Query the identification of the SIM card", "" },
    { "logs",   "DLREC",        cmd_dlrec,              CMD_TYPE_GET, CMD_MODE_RW,    "Download entire/selective logging data from the memory of th device", "" },
    { "ver",    "VER",          cmd_get_template,       CMD_TYPE_GET, CMD_MODE_R,     "Get current firmware version", "Get SW version" },

    /* Get/Set command */
    { "tz",     "SETTZ",        cmd_set_template,       CMD_TYPE_GETSET, CMD_MODE_RW,    "Set the time zone information for the device", ""},
    { "sms",    "SMSM",         cmd_set_template,       CMD_TYPE_GETSET, CMD_MODE_RW,    "Switch the SMS format (Text of PDU mode)", "" },
    { "comm",   "COMMTYPE",     cmd_set_template,       CMD_TYPE_GETSET, CMD_MODE_RW,    "Set/Read device communication type and its parameters", ""},
    { "vip",    "SETVIP",       cmd_set_template,       CMD_TYPE_GETSET, CMD_MODE_RW,    "Preset up to 5 SMS phone numbers for receiving different alerts", "" },
    { "ps",     "PSMT",         cmd_set_template,       CMD_TYPE_GETSET, CMD_MODE_RW,    "Enable/Disable power saving mode", "" },
    { "config", "UNCFG",        cmd_set_template,       CMD_TYPE_GETSET, CMD_MODE_RW,    "Set/Read device ID, Password, and PIN Code of the SIM card", ""},
    { "geofevt","SETEVT",       cmd_set_template,       CMD_TYPE_GETSET, CMD_MODE_RW,    "Enable/Disable/Set/Read GEO-Fencing event", ""},
    { "rec",    "REC",          cmd_set_template,       CMD_TYPE_GETSET, CMD_MODE_RW,    "Enable/Disable/read logging function to the device", "" },
    { "lowbatt","LOWBATT",      cmd_set_template,       CMD_TYPE_GETSET, CMD_MODE_RW,    "Set/Read the internal battery low level alert", ""},

    /* Execute system commands (no arguments) */
    { "stopdl",  "SPDLREC",      cmd_do_template,       CMD_TYPE_DO, CMD_MODE_W,     "Stop downloading logging data from the device", "" },
    { "clearlog","CLREC",        cmd_do_template,       CMD_TYPE_DO, CMD_MODE_W,     "Erase all logging data from the device", "" },
    { "reboot",  "REBOOT",       cmd_do_template,       CMD_TYPE_DO, CMD_MODE_W,     "Restart-up the device", ""},
    { "reset",   "RESET",        cmd_do_template,       CMD_TYPE_DO, CMD_MODE_W,     "Reset all parameters to the manufactory default settings", ""},
    { "test",    "TEST",         cmd_do_template,       CMD_TYPE_DO, CMD_MODE_W,     "Device diagnostic function", "" }

};

const size_t g7command_list_len = sizeof(g7command_list) / sizeof(struct g7command );

/*
 * The following structure and list details all possible arguments to all commands. This is used
 * to question the user for each argument in turn when he wants to execute a specific command.
 */
struct cmdargs {
    const char *srvcmd;       // Command name
    const size_t numargs;     // NUmber of arguments

    const struct {
        char *arglabel; // The text label to prompt user for this arg (excluding selection choice)
        char *argdesc;  // Short description of argument
        int type;       // Type of argument (for error checking), 0=bool, 1=integer, 2=float, 3=selection
        size_t nsel;    // Number of possible selects for this argument (only used with type==3)
        const struct {
            char *val;            // Value to use
            char *selectlabel;  // Human description of this select option
        } select[MAX_SELECT];
    } argl[MAX_ARGS];
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

// All the commands that allows "set" must have their arguments described in this structure.
// It is possible for a command to have zero argument, for example "set led". They must however
// be included here and explicitly be set to have 0 arguments.
// There is no need to include "get" and "do" only commands here since they never take any arguments
static struct cmdargs cmdargs_list[] = {
	{ "sleep", 1, { { "Message mode", "Set how to handle message at when device enters sleep", ARGT_SELECT, 3,
				{ { "1", "log message to device" },
				  { "2", "send back message" },
                                  { "3", "both log and send back message" },
                                }
                       } }
         },
         { "roam",0,{{0}}},
         { "phone",0,{{0}}},
         { "led",0,{{0}}},
         { "mswitch",0,{{0}}},

         { "lowbatt",2,{
                         {"Report action","Action to perform at low battery voltage",ARGT_SELECT,4,{
                                {"0","Disable"},
                                {"1","Log on device"},
                                {"2","Send back to server"},
                                {"3","Both log and send back ro server"}}},
                          {"VIP mask","Bitmask of VIP numbers to alert",ARGT_INT,0,{{0}}}
         }},

         { "sms",1,{{"Mode","Set format for SMS sent back",ARGT_SELECT,2,{{"0","Use PDU mode"},{"1","Use text mode"}}}}},
         { "tz",3,{
                        {"Sign","Ahead or before GMT zone",ARGT_SELECT,2,{{"+","Ahead of GMT"},{"-","Before GMT"}}},
                        {"Hour","Number of hours to adjust (0-9)",ARGT_INT,0,{{0}}},
                        {"Min","Minutes to adjust",ARGT_SELECT,4,{{"0","0 min"},{"15","15 min"},{"30","30 min"},{"45","45 min"}}},
         }},

         { "gfen",5,{
             {AUTO_ENABLE_LABEL,"Turn option on/off",ARGT_BOOL,0,{{0}}},
             {"Radius","Radius in meter for fence",ARGT_INT,0,{{0}}},
             {"Zone trigger","Trigger when leaving or entering zone",ARGT_SELECT,2,{{"1","Inside zone"},{"2","Outside zone"}}},
             {"Report action","What to do on event",ARGT_SELECT,4,{{"0","Disabled"},{"1","Log to device"},{"2","Send to server"},{"3","Both log and send"}}},
             {"VIP mask","Bitmask of VIP numbers to alert",ARGT_INT,0,{{0}}}
         }},

        { "track", 7, {
             { "Tracking mode", "Set condition for when tracking is started", ARGT_SELECT, 9,
				{ { "1", "Time mode" },
				  { "2", "Distance mode" },
                  { "3", "Time AND Distance mode" },
                  { "4", "Time OR distance"},
                  { "5", "Heading mode"},
                  { "6", "Heading OR time"},
                  { "7", "Heading OR distance"},
                  { "8", "Heading OR (Time AND Distance"},
                  { "9", "Heading OR Time OR Distance"}}},
              { "Timer interval", "Used for mode 1", ARGT_INT,0,{{0}}},
              { "Distance interval", "Used for mode 2-9", ARGT_INT,0,{{0}}},
              { "Number of trackings", "Use 0 for continously tracking", ARGT_INT,0,{{0}}},
              { "Track basis","Wait for GPS fix or not",ARGT_SELECT,2,{{"0","Send report ONLY if GPS is fixed"},{"1","Send report regardless of GPS status"}}},
              { "Comm select","Set type of report",ARGT_SELECT,5,{{"0","Over USB"},{"1","Over GSM"},{"2","Over Reserved (not used)"},{"3","UDP over GPRS"},{"4","TCP/IP over GPRS"}}},
              { "Heading", "Used for modes 5-9", ARGT_INT,0,{{0}}},
         } },

         { "comm",10,{
                        {"CommSelect","Select primary type of communication",ARGT_SELECT,5,{{"0","Use USB"},{"1","Use SMS over GSM"},{"2","CSD (reserved)"},{"3","UDP over GPRS"},{"4","TCP/IP over GPRS"}}},
                        {"SMS Base Phone","SMS base number to call",ARGT_INT,0,{{0}}},
                        {"CSD Base Phone","CSD base number (reserved and not used)",ARGT_INT,0,{{0}}},
                        {"GPRS APN","The operators APN",ARGT_STRING,0,{{0}}},
                        {"GPRS User","User name if required",ARGT_STRING,0,{{0}}},
                        {"GPRS Passwd","Password if required",ARGT_STRING,0,{{0}}},
                        {"GPRS Server","Server IP address where the device reports back to",ARGT_STRING,0,{{0}}},
                        {"GPRS Server Port","TCP/IP Port to use on server",ARGT_INT,0,{{0}}},
                        {"GPRS Keep Alive","Interval (in sec) between Keep Alive Packets",ARGT_INT,0,{{0}}},
                        {"GPRS DNS","Optional DNS server for device to use.",ARGT_STRING,0,{{0}}}
         }},

         { "ps",7,{
                        {"Mode","Select when to enable sleep mode to save battery",ARGT_SELECT,4,{
                                {"0","Sleep after 3min of no movement. Wake up on movements. GSM=Standby, GPRS=GPS=Off, G-sensor=On"},
                                {"1","Always enter sleep after 3min and wake every n seconds specified. GSM=GPRS=GPS=G-sensor=Off"},
                                {"2","Always enter sleep after 3min and wake up on timer. GSM=GPRS=GPS=G-sensor=Off"},
                                {"3","Always enter sleep after 3min and wake up on movement. GSM=GPRS=GPS=Off, G-sensor=On"}}},
                        {"Sleep interval","Used with sleep mode 1, Interval in minutes between wakeups",ARGT_INT,0,{{0}}},
                        {"Wake up report","Action when awaken",ARGT_SELECT,4,{
                                {"0","Disable"},
                                {"1","Log on device"},
                                {"2","Send back to server"},
                                {"3","Both log and send back ro server"}}},
                        {"VIP mask","Bitmask of VIP numbers to alert on awake",ARGT_INT,0,{{0}}},
                        {"Timer 1","Used with mode=2. 00-23 hr",ARGT_INT,0,{{0}}},
                        {"Timer 2","Used with mode=2. 00-23 hr",ARGT_INT,0,{{0}}},
                        {"Timer 3","Used with mode=2. 00-23 hr",ARGT_INT,0,{{0}}}
         }},

       { "config",3,{
             {"Device ID","Set device ID (leftmost digit must always be 3)",ARGT_INT,0,{{0}}},
             {"Device Password","Set device Password (numeric)",ARGT_INT,0,{{0}}},
             {"SIM Card PIN","The SIM cards PIN code to use",ARGT_INT,0,{{0}}}
        }},

       { "vip",5,{
             {"VIP 1","Mobile number 1 (full number incl country prefix '+' if needed)",ARGT_STRING,0,{{0}}},
             {"VIP 2","Mobile number 2 (full number incl country prefix '+' if needed)",ARGT_STRING,0,{{0}}},
             {"VIP 3","Mobile number 3 (full number incl country prefix '+' if needed)",ARGT_STRING,0,{{0}}},
             {"VIP 4","Mobile number 4 (full number incl country prefix '+' if needed)",ARGT_STRING,0,{{0}}},
             {"VIP 5","Mobile number 5 (full number incl country prefix '+' if needed)",ARGT_STRING,0,{{0}}},
        }},

      { "geofevt",7,{
                        {"Event ID","Event ID. Maximum of 50 events. In range 50-99",ARGT_INT,0,{{0}}},
                        {"Enabled","Enable/Disable evenet",ARGT_BOOL,0,{{0}}},
                        {"Longitude","Longitude for center of event zone",ARGT_FLOAT,0,{{0}}},
                        {"Latitude","Latitude for center of event zone",ARGT_FLOAT,0,{{0}}},
                        {"Radius","Radius of event zone in meters (0-65535)",ARGT_INT,0,{{0}}},
                        {"ZoneControl","HOw to trigger event",ARGT_SELECT,2,{{"1","Entering zone"},{"2","Leaving zone"}}},
                        {"Action","What to do on event",ARGT_SELECT,3,{{"1","Log to device"},{"2","Send to server"},{"3","Both log and send"}}},
                        {"VIP Phone mask","Bit combination to enable/disable the five VIP numbers",ARGT_INT,0,{{0}}}

         }},

        { "rec",6,{
                        {"Mode","When should logging be done",ARGT_SELECT,10,{
                                {"0","Disable"},
                                {"1","Time mode, log every n seconds"},
                                {"2","Distance mode, log every n meters"},
                                {"3","Time AND Distance mode"},
                                {"4","Time OR Distance mode"},
                                {"5","Heading mode, logging starts when heading > set value"},
                                {"6","Heading OR Time mode"},
                                {"7","Heading OR Distance"},
                                {"8","Heading OR (Time AND Distance)"},
                                {"9","Heading OR Time OR Distance"},
                        }},
                        {"Time interval","Time interval (in sec) used in mode setting",ARGT_INT,0,{{0}}},
                        {"Distance interval","Distance interval (in meters) used in mode setting",ARGT_INT,0,{{0}}},
                        {"Number of reports","The number of reports to send bakc on event (0=continously logging)",ARGT_INT,0,{{0}}},
                        {"Record basis","Logging mode",ARGT_SELECT,2,{
                                 {"0","Wait for GPS fix before sending back report"},
                                 {"1","Don't wait for GPS fix before sending back report"},
                        }},
                        {"Heading","Heading value used in mode (10-90)",ARGT_INT,0,{{0}}}
         }},
};


const size_t cmdargs_list_len = sizeof(cmdargs_list) / sizeof(struct cmdargs);

// Forward prototypes for command handlers to deal with the special case
// when we download the log from the device
int cmd_rec(const int sockd, const int cmdidx, const int gsflag) {
    return 0;
}
int cmd_dlrec(const int sockd, const int cmdidx, const int gsflag) {
    return 0;
}
int cmd_spdlrec(const int sockd, const int cmdidx, const int gsflag) {
    return 0;
}

/*
 * The following are prototypes for the four classes of command handlers that
 * we use. One command handler caters for multiple device commands depending on
 * usage.
 * - Binary commands are commands that the user can enable/disable
 * - Get command are commands where only a read from the device is available
 * - Do commands are commands without arguments but with side effects such as
 *   resetting the device.
 * - Set command is for specifying arguments to a command.
 *
 * Note: A number of commands support both Get/Set calling. In the command list
 * the actual supported modes for a command is specified with the CMD_MODE_xxx flag.
 */
int cmd_binary_template(const int sockd, const int cmdidx, const int gsflag) {
    return 0;
}
int cmd_get_template(const int sockd, const int cmdidx, const int gsflag) {
    return 0;
}
int cmd_do_template(const int sockd, const int cmdidx, const int gsflag) {
    return 0;
}
int cmd_set_template(const int sockd, const int cmdidx, const int gsflag) {
    return 0;
}
int send_cmd_to_device(int sockd, const char *cmd) {
    return 0;
}


/**
 * Get the the index into the argument table corresponding to a specific command
 * @param cmdidx Command index
 * @return -1 on not found, >= 0 teh argument index
 */
int
get_argidx_for_cmdidx(int cmdidx) {
    size_t i = 0;

    //printf("Comparing %s (list_len=%zd)\n",g7command_list[cmdidx].srvcmd,cmdargs_list_len);
    while ( i < cmdargs_list_len && strcmp(g7command_list[cmdidx].srvcmd,cmdargs_list[i].srvcmd) ) {
        //printf("   with %s\n",cmdargs_list[i].srvcmd);
        i++;
    }
    if (i >= cmdargs_list_len) {
        //fprintf(stderr, "Internal error. get_argidx_for_cmdidx(int cmdidx). No argidx for cmdidx=%d",cmdidx);
        return -1;
    }
    return (int)i;
}


int
cmp_cmd(const void *a, const void *b) {
  const struct g7command *ap = a;
  const struct g7command *bp = b;
  return strcmp(ap->srvcmd,bp->srvcmd);
}

void
gen_xml(void) {

  qsort(g7command_list,g7command_list_len,sizeof(struct g7command),cmp_cmd);


    printf("<variablelist>\n");
    for(size_t i = 0; i < g7command_list_len; ++i ) {
        printf("<varlistentry>\n<term><emphasis role=\"bold\">%s</emphasis></term><listitem>\n<para>%s</para>\n", g7command_list[i].srvcmd,g7command_list[i].descrshort);
        int arg = get_argidx_for_cmdidx(i);
        if( arg > -1 && cmdargs_list[arg].numargs > 0 ) {
            printf("<para><itemizedlist>\n");
            for(size_t j=0; j < cmdargs_list[arg].numargs; ++j ) {
                printf("<listitem><para>\n");
                printf("<emphasis>%s</emphasis> - %s\n</para>",cmdargs_list[arg].argl[j].arglabel,cmdargs_list[arg].argl[j].argdesc);

                if( cmdargs_list[arg].argl[j].nsel > 0 ) {
                    printf("<para>Possible values and description:<itemizedlist>\n");

                    for(size_t k=0; k < cmdargs_list[arg].argl[j].nsel; ++k ) {
                        printf("<listitem><para>\n");
                        printf("<parameter>%s</parameter> - \"%s\"\n",cmdargs_list[arg].argl[j].select[k].val,cmdargs_list[arg].argl[j].select[k].selectlabel);
                        printf("</para></listitem>\n");
                    }

                    printf("</itemizedlist></para>\n");
                }
                printf("</listitem>\n");
            }
            printf("</itemizedlist></para>\n");
        }
        printf("</listitem>\n</varlistentry>\n");
    }
    printf("</variablelist>\n");
}


/*
 *
 */
int main(int argc, char** argv) {
    gen_xml();
    return (EXIT_SUCCESS);
}

