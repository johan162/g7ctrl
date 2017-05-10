// ArgsTest.cpp : main project file.

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace System;

#define ARGT_BOOL 0
#define ARGT_INT 1
#define ARGT_FLOAT 2
#define ARGT_SELECT 3
#define MAX_SELECT 8
#define MAX_ARGS 12

struct cmdargs {
	size_t cmdidx;    				  // Index for which command this description is valid
	size_t numargs;	 				  // NUmber of arguments
	struct {
		char *arglabel; 				  // The text label to prompt user for this arg (excluding selection choice)   
		char *argdesc;				// Short description of argument
		int type;                      // Type of argument (for error checking), 0=bool, 1=integer, 2=float, 3=selection
		size_t nsel;                   // Number of possible selects for this argument (only used with type==3)
		struct {
			int val;					  // Numeric value for this select
			char *selectlabel;   	      // Human description of this select option
		} select[MAX_SELECT];
	} argl[MAX_ARGS];
};

struct cmdargs cmdargs_list[] = {
	{ 1, 2, { { "Enabled", "Set function on/off", ARGT_BOOL, 0 }, 
	          { "Timeout", "Timeout in s", ARGT_INT, 0 } } },

    { 2, 2, { { "Enabled", "Set function on/off", ARGT_BOOL, 0 },
			  { "Mode", "Select operating mode", ARGT_SELECT, 2,
				{ { 1, "All the time" },
				  { 2, "Only when active" } } } }
	} 
};

/*
size_t strlen(char *s) {
	size_t n = 0;
	while (*s++) n++;
	return n;
}

int strcat(char *s1, char *s2) {
	char *p = s1+strlen(s1);
	while (*p++ = *s2++);
	*p = '\0';
	return 0;
}
*/

int get_cmd_args(int cmdidx, size_t maxlen, char *buff) {

	size_t i = 0; 
	while (i < sizeof(cmdargs_list) && cmdidx != cmdargs_list[i].cmdidx ) {
		i++;
	}
	if (i >= sizeof(cmdargs_list)) {
		return -1;
	}

	struct cmdargs *p = &cmdargs_list[i];
	char reply[32];

	*buff = '\0';
	for (size_t i=0; i < p->numargs; i++) {
		char info[32] = { '\0' }, tmp[32] = { '\0' };
		switch (p->argl[i].type) {
			case ARGT_BOOL:
				printf("%s - %s\n", p->argl[i].arglabel, p->argl[i].argdesc);
				printf("   0 - Disable\n");
				printf("   1 - Enable\n");
				printf("Select (0/1) : ");
				fscanf(stdin,"%s",reply);
				if (1 != strlen(reply) || (*reply != '0' && *reply != '1')) {
					printf("\"%s\"\n", reply);
					return -1;
				}
				break;
			case ARGT_INT:
				printf("%s - %s\n", p->argl[i].arglabel, p->argl[i].argdesc);
				fscanf(stdin, "%s", reply);
				break;
			case ARGT_FLOAT:
				printf("%s <float> - %s\n", p->argl[i].arglabel, p->argl[i].argdesc);
				fscanf(stdin, "%s", reply);
				break;
			case ARGT_SELECT:
				printf("%s - %s\n", p->argl[i].arglabel, p->argl[i].argdesc);				
				*info = '(';
				for (size_t j = 0; j < p->argl[i].nsel; j++) {
					printf("  %2d - %s\n", p->argl[i].select[j].val, p->argl[i].select[j].selectlabel);
					sprintf_s(tmp, "%d", p->argl[i].select[j].val);
					strcat(info, tmp);
					if (j < p->argl[i].nsel - 1)
						strcat(info, "/");
				}
				strcat(info, ")");
				printf("Select %s : ", info);
				fscanf(stdin, "%s", reply);
				// Check value against all select value
				int val;
				val = atoi(reply);
				int tst;
				tst = false;
				for (size_t t = 0; t < p->argl[i].nsel && !tst; t++) {
					tst = (val == p->argl[i].select[t].val);
				}
				if (!tst) {
					return -1; // Value outside select
				}
				break;
			default:
				return -1;
		}
		if ( i > 0 ) strcat(buff, ",");
		strcat(buff, reply);
		printf("\n");
	}

	return 0;
}



int main(array<System::String ^> ^args)
{
	char buff[64];
	if (0 == get_cmd_args(2, sizeof(buff), buff))
		printf("Result: %s\n", buff);
	else
		printf("Invalid argument!\n");
    return 0;
}
