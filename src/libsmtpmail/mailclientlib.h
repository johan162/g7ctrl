/* =========================================================================
 * File:        MAILCLIENTLIB.H
 * Description: A simple library to provide interface to a SMTP server
 *              and make it easy to send mails. The library supports both
 *              handling of attachment, plain and HTML mail as well as
 *              inline images within the HTML code.
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

#ifndef MAILCLIENTLIB_H
#define	MAILCLIENTLIB_H

#ifdef	__cplusplus
extern "C" {
#endif

/* User agent string used */
#define SMTP_USER_AGENT "libsmtp mailer 1.0"

/* Maximum number of recepients in one mail */
#define MAX_RCPT 50

/* Maximum number of attachments in one mail*/
#define MAX_ATTACHMENTS 20

/* Possible features of the server */
#define SMTP_SERVER_FEATURE_PIPELINING 0
#define SMTP_SERVER_FEATURE_8BITMIME 1
#define SMTP_SERVER_FEATURE_AUTH_PLAIN_LOGIN 2
#define SMTP_SERVER_FEATURE_VERIFY 3
#define SMTP_SERVER_FEATURE_ETRN 4
#define SMTP_SERVER_FEATURE_ENHANCEDSTATUS 5
#define SMTP_SERVER_FEATURE_DSN 6

/* Supported types of attachments */
#define SMTP_ATTACH_CONTENT_TYPE_PLAIN 0
#define SMTP_ATTACH_CONTENT_TYPE_HTML 1
#define SMTP_ATTACH_CONTENT_TYPE_PNG 2
#define SMTP_ATTACH_CONTENT_TYPE_JPG 3
#define SMTP_ATTACH_CONTENT_TYPE_GIF 4
#define SMTP_ATTACH_CONTENT_TYPE_OCTET 5
#define SMTP_ATTACH_CONTENT_TYPE_PDF 6
#define SMTP_ATTACH_CONTENT_TYPE_GZIP 7
#define SMTP_ATTACH_CONTENT_TYPE_ZIP 8
#define SMTP_ATTACH_CONTENT_TYPE_XML 9
#define SMTP_ATTACH_CONTENT_TYPE_CSV 10

/* What encoding is used for the data sent in mail/attachment */
#define SMTP_CONTENT_TRANSFER_ENCODING_8BIT 0
#define SMTP_CONTENT_TRANSFER_ENCODING_BASE64 1
#define SMTP_CONTENT_TRANSFER_ENCODING_QUOTEDPRINT 2

/* Type of recepients of mail */
#define SMTP_RCPT_TO 1
#define SMTP_RCPT_CC 2
#define SMTP_RCPT_BCC 3

/* Maximum size fo each recipient line */
#define MAX_HEADER_ADDR_SIZE 2048

/*
 * Structure to hold the reply from SMTP server for a specific command
 */
struct smtp_reply {
    int status;
    char *str;
};

/**
 * Hold data about one single attachment
 */
struct smtp_attachment {
    char *data;
    char *contenttype;
    char *contenttransferencoding;
    char *contentdisposition;
    char *filename;
    char *name;
    char *cid;
};


/**
 * Structures to hold the state of the SMTP connection as well as storing vital information
 * and data that will be used when formatting the mail to be sent.
 */
struct smtp_handle {
    /** Socket fledescriptor for connection to server */
    int sfd;
    /** Capability index into cap array */
    size_t capidx;
    /** Capabilityies of the server */
    struct smtp_reply * cap[64];
    /** Subject of mail */
    char *subject;
    /** RFC5322 formatted date string for mail*/
    char *date;
    /** All "To:" recipients */
    char *to[MAX_RCPT];
    /** All "To:" recipients as one string */
    char *to_concatenated;
    /** Index into to array */
    size_t toidx;
    /** All "CC:" recipients */
    char *cc[MAX_RCPT];
    /** All "CC:" recipients concatenated as a string */
    char *cc_concatenated;
    /** Index into CC array */
    size_t ccidx;
    /** All "BCC:" recipients */
    char *bcc[MAX_RCPT];
    /** All "BCC:" recipients concatenated as a string */
    char *bcc_concatenated;
    /** Index into BCC array */
    size_t bccidx;
    /** What Envelope "From" address to use */
    char *from;
    /** Explicit return path */
    char *returnpath;
    /** MIME Version used (always 1.0) */
    char *mimeversion;
    /** Array to hold all attachments in mail */
    struct smtp_attachment * attachment[MAX_ATTACHMENTS];
    /** Index into attachment array */
    size_t attachmentidx;
    /** HTML body of message */
    char *html;
    /**  Plain text body of message*/
    char *plain;
    /** User agent string */
    char *useragent;
    /** Content type for envelope*/
    char *contenttype;
    /** Encoding for envelope */
    char *contenttransferencoding;
    /** The overall data buffer that will hold a complete
        data section of the mail. All texts, all attachments etc.
        This is what is actually sent to the SMTP server as
        the payload of the mail.*/
    char *databuff;
};


int
smtp_add_rcpt(struct smtp_handle *handle, unsigned type, char *rcpt);

int
smtp_add_plain(struct smtp_handle *handle, char *buffer);

int
smtp_add_html(struct smtp_handle *handle, char *buffer, char *altbuffer);

int
smtp_add_attachment(struct smtp_handle *handle, char *filename, char *name, char *data, size_t len,
                    unsigned contenttype, unsigned encoding);

int
smtp_add_attachment_fromfile(struct smtp_handle *handle, char *filename, unsigned contenttype, unsigned encoding);

int
smtp_add_attachment_inlineimage(struct smtp_handle *handle, char *filename, char *cid);

int
smtp_add_attachment_inlineimage_buffer(struct smtp_handle *handle, char *imgname, char *imagedata, size_t len, char *imgformat, char *cid);

int
smtp_add_attachment_gzip(struct smtp_handle *handle, char *filename);

int
smtp_add_attachment_binary(struct smtp_handle *handle, char *filename);

int
smtp_sendmail(struct smtp_handle *handle, char *from, char *subject);

int
smtp_server_support(struct smtp_handle *handle, size_t feature);


struct smtp_handle *
smtp_setup(char *server_ip, char *user, char *pwd, int port);


void
smtp_cleanup(struct smtp_handle **handle);

void
smtp_dump_handle(struct smtp_handle * handle, FILE *fp);

int
smtp_simple_sendmail(char *server, char *user, char *pwd,
                     char * subject,
                     char * from, char *to, char *cc,
                     char *message, unsigned isHTML);

int
smtp_simple_sendmail_with_fileattachment(char *server, char *user, char *pwd,
                     char * subject,
                     char * from, char *to, char *cc,
                     char *message, unsigned isHTML,
                     char *filename, int contenttype, int encoding);

#ifdef	__cplusplus
}
#endif

#endif	/* MAILCLIENTLIB_H */

