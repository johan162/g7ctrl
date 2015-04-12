/* =========================================================================
 * File:        MAILUTIL.C
 * Description: Utility function to send mail through command line
 *              or using an SMTP server (through smtplib)
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: mailutil.h 912 2015-04-05 13:30:34Z ljp $
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

#ifndef MAILUTIL_H
#define	MAILUTIL_H


#ifdef	__cplusplus
extern "C" {
#endif

#include "rkey.h"

/**
 * Name of the directory under the main data directory where the mail templates
 * are stored.
 */
#define MAIL_TEMPLATE_SUBDIR "mail_templates"

struct inlineimage_t {
    char *data;
    size_t size;
    char *format;
    char *filename;
};

int
send_mail(const char *subject, const char *from, const char *to, const char *message);

int
send_mail_template(char *subject, char *from, char *to,
        char *templatename, struct keypairs keys[], size_t nkeys, size_t maxkeypairs, char *attFileName, 
        size_t numinline, struct inlineimage_t *inlineimages);

int
sendmail_helper(char *subject,char *buffer_plain,char *buffer_html);

int
sendmail_binattach_helper(char *to, char *subject, char *buffer_plain, char *buffer_html, char *binaryFile);

int
setup_inlineimg(struct inlineimage_t *inlineimg, const char *filename, size_t size, char *imgdata);

void
free_inlineimg_array(struct inlineimage_t *inlineimg, size_t size);

int
read_inlineimg_from_file(char *filename, struct inlineimage_t *inlineimg);

int
tst_mailimg(void);

#ifdef	__cplusplus
}
#endif

#endif	/* MAILUTIL_H */

