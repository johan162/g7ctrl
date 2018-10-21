/* =========================================================================
 * File:        MAILUTIL.C
 * Description: Utility function to send mail through command line
 *              or using an SMTP server (through smtplib)
 * Author:      Johan Persson (johan162@gmail.com)
 *
 * Copyright (C) 2013-2015  Johan Persson
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <libgen.h>
#include <sys/fcntl.h>

#include "config.h"
#include "g7config.h"
#include "utils.h"
#include "dict.h"
#include "libsmtpmail/mailclientlib.h"
#include "logger.h"
#include "mailutil.h"
#include "futils.h"
#include "libxstr/xstr.h"

/**
 * Escape quotes in a string as necessary
 * @param tostr
 * @param fromstr
 * @param maxlen
 * @param remove_nr Removed '\\r', '\\n' in the string
 */
void
escape_quotes(char *tostr, const char *fromstr, size_t maxlen, unsigned remove_nr) {
    size_t i = 0;
    while (i < maxlen - 1 && *fromstr) {
        if (*fromstr == '"') {
            *tostr++ = '\\';
            *tostr++ = '"';
            i += 2;
        } else if (remove_nr && (*fromstr == '\n' || *fromstr == '\r')) {
            *tostr++ = ' ';
            i++;
        } else {
            *tostr++ = *fromstr;
            i++;
        }
        fromstr++;
    }
    *tostr = '\0';
}

/**
 * Send mail using system mail command
 * @param subject
 * @param from
 * @param to
 * @param message
 * @return 0 on success, -1 on failure
 */
int
send_mail(const char *subject, const char *from, const char *to, const char *message) {
    const size_t blen = 20 * 1024;
    char *buffer = calloc(blen, sizeof (char));

    if (strlen(message) >= blen) {
        syslog(LOG_ERR, "Truncating mail sent from daemon 'g7ctrl'");
    }

    const size_t msglen2 = 2 * strlen(message);
    char *qmessage = calloc(msglen2, sizeof (char));
    escape_quotes(qmessage, message, msglen2, 0);

    const size_t sublen2 = 2 * strlen(subject);
    char *qsubject = calloc(sublen2, sizeof (char));
    escape_quotes(qsubject, subject, sublen2, 1);


    if (from == NULL || *from == '\0') {
        snprintf(buffer, blen - 1,
                "echo \"%s\" | /usr/bin/mail -s \"%s\" \"%s\"", qmessage, qsubject, to);
    } else {
        snprintf(buffer, blen - 1,
                "echo \"%s\" | /usr/bin/mail -r \"%s\" -s \"%s\" \"%s\"", qmessage, from, qsubject, to);
    }
    free(qmessage);
    free(qsubject);

    int rc = system(buffer);
    free(buffer);

    if (rc) {
        syslog(LOG_ERR, "Failed to send mail. rc=%d ( %d : %s )", rc, errno, strerror(errno));
    } else {
        logmsg(LOG_DEBUG, "Sent mail to '%s' with subject '%s'", to, subject);
    }

    return rc;

}


/*
static int
get_fext(char *filename,char *fext,size_t maxlen) {
    ssize_t n = strlen(filename) - 1;
    while( n > 0 && filename[n] != '.' ) {
        n--;
    }
    if( n <= 0 || strlen(&filename[n+1]) >= maxlen ) {
        return -1;
    }
    strcpy(fext,&filename[n+1]);
    return 0;
}
 */

/**
 * Send a mail based on a template file. If the template file ends in *.html the mail
 * will be sent as an HTML mail and if a template with the same basename but with
 * extension *.txt exists then that will be used to send a plain text version of the
 * mail. If the template file ends in *.txt then the mail will only be sent as a
 * plain text.
 * If attFileName is != NULL then the named file will be added as an attachment encode
 * using BASE64 encoding and added to the mail.
 * @param subject Mail subject
 * @param from From address
 * @param to To mail address
 * @param templatename The template name (eithout suffix)
 * @param keys The associative map for keywords
 * @param nkeys The current number of keys
 * @param maxkeypairs The maximum number of key pairs allows in the key map
 * @param attFileName The name of a single file attachment to be added to the mail
 * @param numinline Number of inline images in the inlineimages array
 * @param inlineimages A vector of data for the inline images to be added to the mail
 * @return 0 on success, -1 on failure
 */
int
send_mail_template(char *subject, char *from, char *to,
        char *templatename, 
        dict_t keyword_dict, 
        char *attFileName, 
        size_t numinline, struct inlineimage_t *inlineimages) {

    if (!enable_mail) {
        logmsg(LOG_DEBUG, "Mailhandling disabled in configuration. No mail will be sent.");
        return -99;
    }

    char *buffer = NULL, *buffer2 = NULL;
    char templatefile[256];

    logmsg(LOG_DEBUG, "smtp_use=%d, use_html_mail=%d", smtp_use, use_html_mail);

    if (use_html_mail && smtp_use) {
        snprintf(templatefile, sizeof (templatefile), "%s/%s/%s.html", data_dir, MAIL_TEMPLATE_SUBDIR, templatename);
        logmsg(LOG_DEBUG, "Sending HTML message using template: \"%s\" ", templatefile);
    } else {
        snprintf(templatefile, sizeof (templatefile), "%s/%s/%s.txt", data_dir, MAIL_TEMPLATE_SUBDIR, templatename);
        if (use_html_mail) {
            logmsg(LOG_NOTICE, "Cannot send HTMl mail (no SMTP server configured) using plain text instead with template: \"%s\"", templatefile);
        }
        logmsg(LOG_DEBUG, "Sending TEXT message using template: \"%s\" ", templatefile);
    }
    
    // If we have inline images we assume that there are the corresponding number of [CIDnn] keywords in the templates
    // that we need to substitute with proper references
    // <img src="cid:[CID01]">  =>  <img src="cid:img_cid00">
    char cidbuff[32];
    char cidvalbuff[32];
    if( NULL != inlineimages) {
        for(int i=0; i < (int)numinline; i++) {
            snprintf(cidbuff, sizeof(cidbuff)-1, "CID%02d",i+1);
            snprintf(cidvalbuff, sizeof(cidvalbuff)-1, "img_cid%02d",i);        
            add_dict(keyword_dict, cidbuff, cidvalbuff);
        }
    }

    logmsg(LOG_DEBUG, "Replacing keywords in template: \"%s\" ", templatefile);
    buffer = NULL;
    int rc = replace_dict_in_file(keyword_dict, templatefile, &buffer);
    if (-1 == rc) {
        logmsg(LOG_ERR, "Failed to do keyword substitution with template: \"%s\". Does it exist?", templatefile);
        if (buffer) free(buffer);
        return -1;
    }

    if (!smtp_use || !use_html_mail) {
        logmsg(LOG_DEBUG, "Sending mail via system mail command.");
        rc = send_mail(subject, from, to, buffer);
        free(buffer);
        return rc;
    }

    if (use_html_mail) {
        // Also try to get a plain text version
        snprintf(templatefile, sizeof (templatefile), "%s/%s/%s.txt", data_dir, MAIL_TEMPLATE_SUBDIR, templatename);
        logmsg(LOG_DEBUG, "Getting a plain text version of the HTML template: %s", templatefile);
        rc = replace_dict_in_file(keyword_dict, templatefile, &buffer2);

        if (-1 == rc) {
            logmsg(LOG_DEBUG, "Could not find a plain text version of the template '%s'", templatefile);
        }
    }

    struct smtp_handle *handle = smtp_setup(smtp_server, smtp_user, smtp_pwd, smtp_port);

    if (handle == NULL) {
        logmsg(LOG_ERR, "Could NOT connect to SMTP server (%s) with credentials [%s:%s] on port %d", smtp_server, smtp_user, smtp_pwd, smtp_port);
        free(buffer);
        if (buffer2)
            free(buffer2);
        return -1;
    }

    logmsg(LOG_DEBUG, "Connected to SMTP server (%s) with credentials [%s:%s] on port %d", smtp_server, smtp_user, smtp_pwd, smtp_port);
    rc = smtp_add_rcpt(handle, SMTP_RCPT_TO, to);
    if (-1 == rc) {
        logmsg(LOG_ERR, "Could NOT add To: '%s'", to);
        free(buffer);
        if (buffer2)
            free(buffer2);
        smtp_cleanup(&handle);
        return -1;
    } else {
        logmsg(LOG_DEBUG, "Added recipients To: '%s'", to);
    }

    if (use_html_mail) {
        rc = smtp_add_html(handle, buffer, buffer2);
    } else {
        rc = smtp_add_plain(handle, buffer);
    }

    if (-1 == rc) {
        logmsg(LOG_ERR, "Could NOT add body text to mail.");
    }

    free(buffer);
    if (buffer2) {
        free(buffer2);
    }

    if (attFileName && *attFileName) {

        if (-1 == smtp_add_attachment_binary(handle, attFileName)) {
            logmsg(LOG_ERR, "Cannot add binary file attachment \"%s\" to mail", attFileName);
        } else {
            logmsg(LOG_DEBUG, "Added attachment from file \"%s\"", attFileName);
        }

    }
    
    if( NULL != inlineimages) {
        for(int i=0; i < (int)numinline; i++) {
            snprintf(cidvalbuff,sizeof(cidvalbuff),"img_cid%02d",i);
            smtp_add_attachment_inlineimage_buffer(handle,
                                                   inlineimages[i].filename,
                                                   inlineimages[i].data,
                                                   inlineimages[i].size,
                                                   inlineimages[i].format,
                                                   cidvalbuff);
        }
    }

    rc = smtp_sendmail(handle, from, subject);

    if (-1 == rc) {
        logmsg(LOG_ERR, "Could NOT send mail with subject '%s' using SMTP server !", subject);
    } else {
        logmsg(LOG_DEBUG, "Successfully sent SMTP mail with subject '%s' ", subject);
    }

    smtp_cleanup(&handle);
    return rc;
}

/**
 * Send mail with both HTML and alternative plain text format. The to and from
 * address are taken from the config file
 *
 * @param subject
 * @param buffer_plain
 * @param buffer_html
 * @return 0 on success, -1 on failure
 */
int
sendmail_helper(char *subject, char *buffer_plain, char *buffer_html) {
    int rc;
    if (!smtp_use || !use_html_mail) {

        logmsg(LOG_DEBUG, "Sendmail_helper: Using system mail command.");
        rc = send_mail(subject, daemon_email_from, send_mailaddress, buffer_plain);

    } else {

        logmsg(LOG_DEBUG, "Sendmail_helper: Using SMTP server");
        struct smtp_handle *handle = smtp_setup(smtp_server, smtp_user, smtp_pwd, smtp_port);
        if (handle == NULL) {
            logmsg(LOG_ERR, "Could NOT connect to SMTP server (%s) with credentials [%s:%s] on port %d", smtp_server, smtp_user, smtp_pwd, smtp_port);
            return -1;
        }
        logmsg(LOG_DEBUG, "Sendmail_helper: Connected to SMTP server '%s' (on port %d)", smtp_server, smtp_port);

        if (-1 == smtp_add_html(handle, buffer_html, buffer_plain)) {
            logmsg(LOG_ERR, "Could NOT add content in mail");
            smtp_cleanup(&handle);
            return -1;
        }
        logmsg(LOG_DEBUG, "Sendmail_helper: Added plain and HTML content");

        if (-1 == smtp_add_rcpt(handle, SMTP_RCPT_TO, send_mailaddress)) {
            logmsg(LOG_ERR, "Could NOT add recepient to mail");
            smtp_cleanup(&handle);
            return -1;
        }
        logmsg(LOG_DEBUG, "Sendmail_helper: Added recipient '%s'", send_mailaddress);

        rc = smtp_sendmail(handle, daemon_email_from, subject);
        if (-1 == rc) {
            logmsg(LOG_ERR, "could not SEND mail via SMTP.");
        }

        smtp_cleanup(&handle);
        logmsg(LOG_DEBUG, "Sendmail_helper: Cleaned up and sent mail successfully.");
    }

    return rc;
}

/**
 *
 * Send mail with both HTML, alternative plain text format and binary attachment from specified
 * file. The "From:" is taken from the config file.
 *
 * @param to
 * @param subject
 * @param buffer_plain
 * @param buffer_html
 * @param binaryFile
 * @return 0 on success, -1 on failure
 */
int
sendmail_binattach_helper(char *to, char *subject, char *buffer_plain, char *buffer_html, char *binaryFile) {
    int rc;
    if (!smtp_use || !use_html_mail) {

        logmsg(LOG_DEBUG, "Sendmail_helper: Using system mail command.");
        rc = send_mail(subject, daemon_email_from, send_mailaddress, buffer_plain);

    } else {

        logmsg(LOG_DEBUG, "Sendmail_helper: Using SMTP server");
        struct smtp_handle *handle = smtp_setup(smtp_server, smtp_user, smtp_pwd, smtp_port);
        if (handle == NULL) {
            logmsg(LOG_ERR, "Could NOT connect to SMTP server (%s) with credentials [%s:%s] on port %d", smtp_server, smtp_user, smtp_pwd, smtp_port);
            return -1;
        }
        logmsg(LOG_DEBUG, "Sendmail_helper: Connected to SMTP server '%s' (on port %d)", smtp_server, smtp_port);

        if (-1 == smtp_add_html(handle, buffer_html, buffer_plain)) {
            logmsg(LOG_ERR, "Could NOT add content in mail");
            smtp_cleanup(&handle);
            return -1;
        }
        logmsg(LOG_DEBUG, "Sendmail_helper: Added plain and HTML content");

        if (-1 == smtp_add_rcpt(handle, SMTP_RCPT_TO, to)) {
            logmsg(LOG_ERR, "Could NOT add recepient to mail");
            smtp_cleanup(&handle);
            return -1;
        }
        logmsg(LOG_DEBUG, "Sendmail_helper: Added recipient '%s'", to);

        if (-1 == smtp_add_attachment_binary(handle, binaryFile)) {
            logmsg(LOG_ERR, "Could not add binary attachment to mail");
            smtp_cleanup(&handle);
            return -1;
        }

        rc = smtp_sendmail(handle, daemon_email_from, subject);
        if (-1 == rc) {
            logmsg(LOG_ERR, "could not SEND mail via SMTP.");
        }

        smtp_cleanup(&handle);
        logmsg(LOG_DEBUG, "Sendmail_helper: Cleaned up and sent mail successfully.");
    }

    return rc;
}


/**
 * Setup the inlineimage structure from a file name and imagedata
 * @param inlineimg A pointer to the inlineimage structure to be filled out
 * @param name The filename. The image format is determined by the file suffix.
 * @param size The size of the image data
 * @param imgdata The raw imagedata
 * @return o on success, -1 on failure
 */
int
setup_inlineimg(struct inlineimage_t *inlineimg, const char *filename, size_t size, char *imgdata) {
    char *t1_filename = strdup(filename);
    char *t2_filename = basename(t1_filename);
    char *bfilename = strdup(t2_filename);
    free(t1_filename);
       
    // find out file suffix
    char suffix[8];
    char *t3_filename = strdup(bfilename);
    strip_filesuffix(t3_filename, suffix, sizeof(suffix)); 
    xstrtolower(suffix);

    inlineimg->data = imgdata;
    inlineimg->filename = strdup(bfilename);
    inlineimg->size = size;
    inlineimg->format = strdup(suffix);

    free(t3_filename);    
    free(bfilename);
    
    return 0;
}

/**
 * Helper function to read an image from file into an inline image buffer tp pass on to
 * the construction of a mail 
 * @param filename Filename and path for the image file
 * @param inlineimg A pointer to an inline image structre to be filled with image
 * meta data and raw image data
 * @return 0 on success, -1 on failure
 */
int
read_inlineimg_from_file(char *filename, struct inlineimage_t *inlineimg) {
    char *buffer;
    size_t readlen;
        
    int rc = read_file_buffer(filename, &readlen, &buffer);
    if( -1 == rc ) {
        return -1;
    }
    
    return setup_inlineimg(inlineimg, filename, readlen, buffer);

}

/**
 * Helper function to free tha structure that holds the inlineimage data
 * @param inlineimg The inlineimage data
 * @param size The number of inlineimages
 */
void
free_inlineimg_array(struct inlineimage_t *inlineimg, size_t size) {
    for(size_t i=0; i < size; i++) {
        //free(inlineimg[i].data);
        free(inlineimg[i].filename);
        free(inlineimg[i].format);
    }
}

/* EOF */
