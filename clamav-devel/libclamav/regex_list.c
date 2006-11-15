/*
 *  Match a string against a list of patterns/regexes.
 *
 *  Copyright (C) 2006 T�r�k Edvin <edwintorok@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 *
 *  $Log: regex_list.c,v $
 *  Revision 1.15  2006/11/15 15:26:54  tkojm
 *  pattern matcher accuracy improvements
 *
 *  Revision 1.14  2006/11/05 18:16:56  acab
 *  Patch for bug 52 from Edvin
 *
 *  Revision 1.13  2006/10/30 17:53:03  tkojm
 *  apply patch from Edvin reported by Luca
 *
 *  Revision 1.12  2006/10/28 15:34:40  tkojm
 *  .pdb/.wdb files now use colon as delimiter
 *
 *  Revision 1.11  2006/10/15 19:16:33  tkojm
 *  allow loading multiple .pdb/.wdb files
 *
 *  Revision 1.10  2006/10/14 23:52:02  tkojm
 *  code cleanup
 *
 *  Revision 1.9  2006/10/10 23:51:49  tkojm
 *  apply patches for the anti-phish code from Edwin
 *
 *  Revision 1.8  2006/10/08 18:55:15  tkojm
 *  fix crash in phishing code on database reload (Edvin Torok)
 *
 *  Revision 1.7  2006/10/07 11:00:46  tkojm
 *  make the experimental anti-phishing code more thread safe
 *
 *  Revision 1.6  2006/09/27 19:14:49  njh
 *  Fix segfault on Solaris
 *
 *  Revision 1.5  2006/09/26 18:55:36  njh
 *  Fixed portability issues
 *
 *  Revision 1.4  2006/09/25 18:27:00  njh
 *  Fix handling of escaped characters
 *
 *  Revision 1.3  2006/09/24 19:28:03  acab
 *  fixes for type "R" regex handler
 *
 *  Revision 1.2  2006/09/16 15:49:27  acab
 *  phishing: fixed bugs and updated docs
 *
 *  Revision 1.1  2006/09/12 19:38:39  acab
 *  Phishing module merge - libclamav
 *
 *  Revision 1.13  2006/09/11 19:25:08  edwin
 *  Non-printable characters in regex (although they are invalid inside an url, added some support for it).
 *
 *  Revision 1.12  2006/08/28 08:43:06  edwin
 *  Fixed a few minor leaks.
 *  Valgrind now says:"All heap blocks were freed -- no leaks are possible"
 *
 *  Revision 1.11  2006/08/20 21:18:11  edwin
 *  Added the script used to generate iana_tld.sh
 *  Added checks for phish_domaincheck_db
 *  Added phishing module design document from wiki (as discussed with aCaB).
 *  Updated .wdb/.pdb format documentation (in regex_list.c)
 *  Fixed some memory leaks in regex_list.c
 *  IOW: cleanups before the deadline.
 *  I consider my module to be ready for evaluation now.
 *
 *  Revision 1.10  2006/08/20 19:42:02  edwin
 *  Fix custom character class, and generic regex handling.
 *
 *  Revision 1.9  2006/08/19 21:08:47  edwin
 *  Fixed:Forgot to add form tag handling when it contains images.
 *  Various fixes to get rid of gcc warnings.
 *
 *  Revision 1.8  2006/08/19 09:26:51  edwin
 *  regex_list.c: Fixed regex alternatives handling (bug discovered with autotests).
 *  And forgot to commit manager.c last time.
 *
 *  Revision 1.7  2006/08/17 20:31:43  edwin
 *  Disable extracting hrefs from mails in mbox, if: we aren't scanning for phish, and mailfollowurls is off.
 *  Fix a still reachable leak. Remove unneeded build_regex_list export.
 *
 *  Revision 1.6  2006/08/12 14:35:34  edwin
 *  Fix some compiler warnings.
 *  Fix an assertion failure in regex_list.
 *  Interpret display links that start with http|https|ftp, always as an URL.
 *
 *  Revision 1.5  2006/08/06 20:27:07  edwin
 *  New option to enable phish scan for all domains (disabled by default).
 *  You will now have to run clamscan --phish-scan-alldomains to have any phishes detected.
 *  Updated phishcheck control flow to better incorporate the domainlist.
 *  Updated manpage with new options.
 *
 *  TODO:there is a still-reachable leak in regex_list.c
 *
 *  Revision 1.4  2006/08/01 20:19:15  edwin
 *  Integrate domainlist check into phishcheck. Warning: enabled by default.
 *  Regex bracket handling update.
 *  Better regex paranthesized & alternate expression handling.
 *
 *  Revision 1.3  2006/07/31 20:12:30  edwin
 *  Preliminary support for domain databases (domains to check by phishmodule)
 *  Better memory allocation failure handling in regex_list
 *
 */

#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif

#ifdef CL_EXPERIMENTAL

#ifndef CL_DEBUG
#define NDEBUG
#endif

#ifdef CL_THREAD_SAFE
#ifndef _REENTRANT
#define _REENTRANT
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#ifdef	HAVE_STRINGS_H
#include <strings.h>
#endif
#include <ctype.h>

#include <limits.h>
#include <sys/types.h>

#ifdef	HAVE_REGEX_H
/*#define USE_PCRE*/
#include <regex.h>
#endif

#if defined(HAVE_READDIR_R_3) || defined(HAVE_READDIR_R_2)
#include <stddef.h>
#endif

#include "clamav.h"
#include "others.h"
#include "defaults.h"
#include "str.h"
#include "filetypes.h"
#include "mbox.h"
#include "regex_list.h"
#include "matcher-ac.h"


/*Tree*/
enum token_op_t {OP_CHAR,OP_STDCLASS,OP_CUSTOMCLASS,OP_DOT,OP_LEAF,OP_ROOT,OP_PARCLOSE};
typedef unsigned char* char_bitmap_p;
/*
 *
 * OP_CHAR: 1 character, c = character
 * complex stuff:
 * OP_STDCLASS: standard character class, c = char class, class: 1<<(index into std_class of class name)
 * OP_CUSTOMCLASS: custom character class, first pointer in ptr array is a pointer to the bitmap table for this class
 * OP_DOT: single . matching any character except \n
 * OP_LEAF: this is a leaf node, reinterpret structure
 */
struct tree_node {
	struct tree_node* next;/* next regex/complex sibling, or parent, if no more siblings , can't be NULL except for root node*/
	unsigned char c;
	enum token_op_t op;
	char alternatives;/* number of (non-regex) children of node, i.e. sizeof(children)*/
	char listend;/* no more siblings, next pointer is pointer to parent*/
	union {
		struct tree_node** children;/* alternatives nr. of children, followed by (a null pointer terminated) regex leaf node pointers) */
		char_bitmap_p* bitmap;
		struct leaf_info*  leaf;
	} u;
};

struct leaf_info {
	char* info;/* what does it mean that we reached the leaf...*/
	regex_t* preg;/* this is NULL if leaf node, and non-regex*/
};

/* Character classes */
static const char* std_class[] = {
	"[:alnum:]",
	"[:digit:]",
	"[:punct:]",
	"[:alpha:]",
	"[:graph:]",
	"[:space:]",
	"[:blank:]",
	"[:lower:]", 
	"[:upper:]",
	"[:cntrl:]",
	"[:print:]",
	"[:xdigit:]"
	/* don't change the order of these strings, unless you change them in generate_tables.c too, and regenerate the tables*/
};


#define STD_CLASS_CNT sizeof(std_class)/sizeof(std_class[0])

/* generated by contrib/phishing/generate_tables.c */
static const unsigned char char_class_bitmap[STD_CLASS_CNT][32] = {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x03, 
         0xfe, 0xff, 0xff, 0x07, 0xfe, 0xff, 0xff, 0x07, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x03, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

        {0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x00, 0xfc, 
         0x01, 0x00, 0x00, 0xf8, 0x01, 0x00, 0x00, 0x78, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0xfe, 0xff, 0xff, 0x07, 0xfe, 0xff, 0xff, 0x07, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

        {0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

        {0x00, 0x3e, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

        {0x00, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0x07, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0xfe, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

        {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

        {0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 
         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x03, 
         0x7e, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

static const unsigned short int char_class[256] = {
        0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x260, 0x220, 0x220, 0x220, 0x220, 0x200, 0x200, 
        0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 
        0x460, 0x414, 0x414, 0x414, 0x414, 0x414, 0x414, 0x414, 0x414, 0x414, 0x414, 0x414, 0x414, 0x414, 0x414, 0x414, 
        0xc13, 0xc13, 0xc13, 0xc13, 0xc13, 0xc13, 0xc13, 0xc13, 0xc13, 0xc13, 0x414, 0x414, 0x414, 0x414, 0x414, 0x414, 
        0x414, 0xd19, 0xd19, 0xd19, 0xd19, 0xd19, 0xd19, 0x519, 0x519, 0x519, 0x519, 0x519, 0x519, 0x519, 0x519, 0x519, 
        0x519, 0x519, 0x519, 0x519, 0x519, 0x519, 0x519, 0x519, 0x519, 0x519, 0x519, 0x414, 0x414, 0x414, 0x414, 0x414, 
        0x414, 0xc99, 0xc99, 0xc99, 0xc99, 0xc99, 0xc99, 0x499, 0x499, 0x499, 0x499, 0x499, 0x499, 0x499, 0x499, 0x499, 
        0x499, 0x499, 0x499, 0x499, 0x499, 0x499, 0x499, 0x499, 0x499, 0x499, 0x499, 0x414, 0x414, 0x414, 0x414, 0x200, 
        0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 
        0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 
        0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 
        0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 
        0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 
        0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 
        0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 
        0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000
};

static const size_t std_class_cnt =  sizeof(std_class)/sizeof(std_class[0]);

/* Prototypes */
static int add_pattern(struct regex_matcher* matcher,const unsigned char* pat,const char* info);
static int match_node(struct tree_node* node,const unsigned char* c,size_t len,const char** info);
static void destroy_tree(struct regex_matcher* matcher);
static struct tree_node* tree_root_alloc(void);
static int build_regex_list(struct regex_matcher* matcher);
static void stack_destroy(struct node_stack* stack);

#ifndef NDEBUG
void dump_tree(struct tree_node* root);
#endif

#define MATCH_SUCCESS 0 
#define MATCH_FAILED  -1

/*
 * Call this function when an unrecoverable error has occured, (instead of exit).
 */
static void fatal_error(struct regex_matcher* matcher)
{
	regex_list_done(matcher);
	matcher->list_inited = -1;/* the phishing module will know we tried to load a whitelist, and failed, so it will disable itself too*/
}


/*
 * @matcher - matcher structure to use
 * @real_url - href target
 * @display_url - <a> tag contents
 * @hostOnly - if you want to match only the host part
 * @is_whitelist - is this a lookup in whitelist?
 *
 * @return - CL_SUCCESS - url doesn't match
 *         - CL_VIRUS - url matches list
 *
 * Do not send NULL pointers to this function!!
 *
 */
int regex_list_match(struct regex_matcher* matcher,const char* real_url,const char* display_url,int hostOnly,const char** info,int is_whitelist)
{
	massert(matcher);
	massert(real_url);
	massert(display_url);
	massert(info);
	if(!matcher->list_inited)
		return 0;
	massert(matcher->list_built);
	{
		size_t real_len    = strlen(real_url);
		size_t display_len = strlen(display_url);
		size_t buffer_len  = (hostOnly && !is_whitelist) ? real_len : real_len + display_len + 1;
		char*  buffer = cli_malloc(buffer_len+1);
		size_t i;
		int rc = 0;
		struct cli_ac_data mdata;

		if(!buffer)
			return CL_EMEM;

		strncpy(buffer,real_url,real_len);
		buffer[real_len]= (!is_whitelist && hostOnly) ? '\0' : ':';
		if(!hostOnly || is_whitelist) {
			strncpy(buffer+real_len+1,display_url,display_len);
			buffer[buffer_len]=0;
		}
		cli_dbgmsg("Looking up in regex_list: %s\n", buffer);

		if(hostOnly) {
			if((rc = cli_ac_initdata(&mdata, 0, AC_DEFAULT_TRACKLEN)))
				return rc;
			rc = 0;

			for(i = 0; i < matcher->root_hosts_cnt; i++) {
				if(( rc = cli_ac_scanbuff((unsigned char*)buffer,buffer_len,info, &matcher->root_hosts[i] ,&mdata,0,0,0,-1,NULL) ))
					break;
			}
		} else
			rc = 0;
    
		if(!rc && !hostOnly) 
			rc = match_node(matcher->root_regex,(unsigned char*)buffer,buffer_len,info) == MATCH_SUCCESS ? CL_VIRUS : CL_SUCCESS;
		free(buffer);
		if(!rc)
			cli_dbgmsg("not in regex list\n");
		return rc;
	}
}

/* node stack */
#define NODE_STACK_INITIAL 1024
#define NODE_STACK_GROW    4096
/* Initialize @stack */
static int stack_init(struct node_stack* stack)
{
	massert(stack);

	stack->cnt = 0;
	stack->capacity = NODE_STACK_INITIAL;
	stack->data = cli_malloc(stack->capacity * sizeof(*stack->data));
	if(!stack->data)
		return CL_EMEM;
	else
		return CL_SUCCESS;
}

/* Reset @stack pointer, but don't realloc */
static void stack_reset(struct node_stack* stack)
{
	massert(stack);

	stack->cnt = 0;
}

/* Push @node on @stack, growing it if necessarry */
static inline int stack_push(struct node_stack* stack,struct tree_node* node)
{
	massert(stack);
	massert(stack->data);

	if(stack->cnt == stack->capacity) {
		stack->capacity += NODE_STACK_GROW;
		stack->data = cli_realloc(stack->data,stack->capacity*sizeof(*stack->data));
		if(!stack->data)
			return CL_EMEM;
	}
	stack->data[stack->cnt++] = node;
	return CL_SUCCESS;
}

/* Pops node from @stack, doesn't realloc */
static inline struct tree_node* stack_pop(struct node_stack* stack)
{
	massert(stack);
	massert(stack->data);
	massert(stack->cnt);/*don't pop from empty stack */

	return stack->cnt ? stack->data[--stack->cnt] : NULL;
}

/* Initialization & loading */

/* Initializes @matcher, allocating necesarry substructures */
int init_regex_list(struct regex_matcher* matcher)
{
	int rc;
	/*
	if(!engine_ok) {
		cli_dbgmsg("Matcher engine not initialized\n");
		return CL_ENULLARG;
	}
	*/

	massert(matcher);
	matcher->list_inited = 0;
 	matcher->root_hosts_cnt = 0;
 	matcher->root_hosts = NULL;
 	matcher->root_hosts_cnt = 0;

	matcher->root_regex = tree_root_alloc();
	if(!matcher->root_regex) {
		return CL_EMEM;
	}

	if(( rc = stack_init(&matcher->node_stack) )) {
		free(matcher->root_regex);
		return rc;
	}
	if(( rc = stack_init(&matcher->node_stack_alt) )) {
		free(matcher->root_regex);
		stack_destroy(&matcher->node_stack);
		return rc;
	}

	matcher->list_inited=1;
	matcher->list_built=1;/* its empty, but pretend its built, so that load_ will realloc root_hosts */
	matcher->list_loaded=0;

	return CL_SUCCESS;
}

/* inserts @pattern into @root, using ac-matcher 
 * although the name might be confusing, @pattern is not a regex!*/
static int add_regex_list_element(struct cli_matcher* root,const char* pattern,char* info)
{
       int ret;
       struct cli_ac_patt *new = cli_calloc(1,sizeof(*new));
       size_t len,i;

       if(!new)
	       return CL_EMEM;
       massert(root);
       massert(pattern);

       len = strlen(pattern);
       new->type = 0;
       new->sigid = 0;
       new->parts = 0;
       new->partno = 0;
       new->mindist = 0;
       new->maxdist = 0;
       new->offset = 0;
       new->target = 0;
       new->length = len;
       if(new->length > root->maxpatlen)
               root->maxpatlen = new->length;

       new->pattern = cli_malloc(sizeof(new->pattern[0])*len);
       if(!new->pattern) {
	       free(new);
	       return CL_EMEM;
       }
       for(i=0;i<len;i++)
	       new->pattern[i]=pattern[i];/*new->pattern is short int* */

       new->virname = strdup(info);
       if((ret = cli_ac_addpatt(root,new))) {
	       free(new->virname);
               free(new->pattern);
               free(new);
               return ret;
       }
       return CL_SUCCESS;
}

/* Load patterns/regexes from file */
int load_regex_matcher(struct regex_matcher* matcher,FILE* fd,unsigned int options,int is_whitelist)
{
	int rc,line=0;
	char buffer[FILEBUFF];

	massert(matcher);
	massert(fd);

	if(matcher->list_inited==-1)
		return CL_EMALFDB; /* already failed to load */
/*	if(matcher->list_loaded) {
		cli_warnmsg("Regex list has already been loaded, ignoring further requests for load\n");
		return CL_SUCCESS;
	}*/
	if(!fd) {
		cli_errmsg("Unable to load regex list (null file)\n");
		return CL_EIO;
	}

	cli_dbgmsg("Loading regex_list\n");
	if(!matcher->list_inited) {
		rc = init_regex_list(matcher);
		if (!matcher->list_inited) {
			cli_errmsg("Regex list failed to initialize!\n");
			fatal_error(matcher);
			return rc;
		}
		/*atexit(regex_list_done); TODO: destroy this in manager.c */
	}
	/*
	 * Regexlist db format (common to .wdb(whitelist) and .pdb(domainlist) files:
	 * Multiple lines of form, (empty lines are skipped):
 	 * Flags RealURL DisplayedURL
	 * Where:
	 * Flags: R - regex, H - host-only, followed by (optional) 3-digit hexnumber representing 
	 * flags that should be filtered.
	 * [i.e. phishcheck urls.flags that we don't want to be done for this particular host]
	 * Note:Flag filtering only makes sense in .pdb files.
	 *
	 * If a line in the file doesn't conform to this format, loading fails
	 * 
	 */
	while(fgets(buffer,FILEBUFF,fd)) {
		char* pattern;
		char* flags;
		line++;
		cli_chomp(buffer);
		if(!*buffer)
			continue;/* skip empty lines */
		pattern = strchr(buffer,':');
		if(!pattern) {
			cli_errmsg("Malformed regex list line %d\n",line);
			fatal_error(matcher);
			return CL_EMALFDB;
		}
		pattern[0]='\0';
		flags = buffer+1;
		pattern++;
		if((buffer[0] == 'R' && !is_whitelist) || (buffer[0] == 'X' && is_whitelist)) {/*regex*/
			if(( rc = add_pattern(matcher,(const unsigned char*)pattern,flags) ))
				return rc==CL_EMEM ? CL_EMEM : CL_EMALFDB;
		}
		else if( ( buffer[0] == 'H' && !is_whitelist) || (buffer[0] == 'M' && is_whitelist)) {/*matches displayed host*/
 			if(matcher->list_built) {
 				struct cli_matcher* old_hosts = matcher->root_hosts;
 				matcher->root_hosts_cnt++;
 
 				matcher->root_hosts = cli_realloc(matcher->root_hosts, matcher->root_hosts_cnt * sizeof(*matcher->root_hosts));
 				if(!matcher->root_hosts) {
 					matcher->root_hosts = old_hosts;/* according to manpage this must still be valid*/
 					return CL_EMEM;
		} 
 				memset(&matcher->root_hosts[matcher->root_hosts_cnt-1], 0, sizeof(struct cli_matcher));
 				matcher->root_hosts[matcher->root_hosts_cnt-1].ac_root = cli_calloc(1, sizeof(struct cli_ac_node));
 				if(!matcher->root_hosts[matcher->root_hosts_cnt-1].ac_root) {
 					matcher->root_hosts_cnt--;
 					return CL_EMEM;
 				}
 				cli_dbgmsg("Increased number of root_hosts in regex_list.c\n");
 				matcher->list_built = 0;
 			}
 			if(( rc = add_regex_list_element(&matcher->root_hosts[matcher->root_hosts_cnt-1],pattern,flags) ))
				return rc==CL_EMEM ? CL_EMEM : CL_EMALFDB;
		}
		else {
			return CL_EMALFDB;
			/* this is useless, we have host, and regex matches
			if(( rc = add_regex_list_element(matcher->root_urls,pattern,flags) ))
				return rc==CL_EMEM ? CL_EMEM : CL_EMALFDB;*/
		}
	}
	matcher->list_loaded = 1;
	if(( rc = build_regex_list(matcher) ))
		return rc;

#ifndef NDEBUG
/*			dump_tree(matcher->root_regex);*/
#endif
	if(!matcher->list_built) {
		cli_errmsg("Regex list not loaded: build failed!\n");
		fatal_error(matcher);
		return CL_EMALFDB;
	}
	regex_list_cleanup(matcher);
	return CL_SUCCESS;
}

/*
static void tree_node_merge_nonbin(struct tree_node* into,const struct tree_node* node)
{
	massert(into);
	massert(node);

	if(node->alternatives){
		if(node->u.children[0]->next == node) {
			*no non-bin alternatives here*
		}
		else {
			struct tree_node* p;
			for(p = node->u.children[0]->next; p->next != node; p = p->next)
				tree_node_insert_nonbin(into,p);
		}
	}
	else
		tree_node_insert_nonbin(into,node->u.children[0]);
}
*
static void tree_node_merge_bin(struct tree_node* into,const struct tree_node* node)
{
	if(node->u.children && node->alternatives) {
		if(!into->alternatives) {
			* into has no bin part, just copy+link the node there*
			int i;
			struct tree_node* next = into->u.children[0];
			into->u.children = node->u.children;
			into->alternatives = node->alternatives;
			for(i=0;i < into->alternatives;i++) {
				if(into->u.children[i]->next == node) {
					into->u.children[i]->next = next;
					into->u.children[i]->listend = 0;
				}
				else {
					struct tree_node* p;
					for(p = into->u.children[0]->next; p->next != node; p = p->next);
					p->listend = 0;
					p->next = next;
				}
			}
		}
		const size_t new_size = tree_node_get_array_size(into) + tree_node_get_array_size(node);
		struct tree_node** new_children = cli_malloc(sizeof(
	}
	* else: no bin part to merge *
}
*/

static struct tree_node ** tree_node_get_children(const struct tree_node* node)
{
	return node->op==OP_CUSTOMCLASS ? (node->u.children[1] ? node->u.children+1 : NULL) :node->u.children;
}
/* don't do this, it wastes too much memory, and has no benefit
static void regex_list_dobuild(struct tree_node* called_from,struct tree_node* node)
{
	struct tree_node **children;
	massert(node);

	children = tree_node_get_children(node);
	if(node->op!=OP_ROOT)
		massert(called_from);
	if(node->op==OP_TMP_PARCLOSE) {
		const size_t array_size = (node->alternatives +(called_from->op==OP_CUSTOMCLASS ? 1:0))*sizeof(*called_from->u.children);
		if(node->c)
			return;* already processed this common node*
		else
			node->c = 1;
		* copy children to called_from from this node
		 * called_from should have 0 alternatives, and a link to this node via ->u.children[0]
		 * *
		massert(called_from->alternatives == 0);
		massert(called_from->u.children);
		massert(called_from->u.children[0] == node);
		called_from->u.children = cli_realloc(called_from->u.children,array_size);
		called_from->u.children = node->u.children;
		called_from->alternatives = node->alternatives;
		if(called_from->alternatives) {
			* fix parent pointers *
			int i;TODO: do a deep copy of children here
			struct tree_node **from_children = tree_node_get_children(called_from);
                        massert(from_children);
			for(i=0;i < called_from->alternatives;i++) {
				struct tree_node* p;
				for(p=from_children[i];p->next != node; p = p->next);
				p->next = called_from;
			}
		}
	}

	if(node->op==OP_LEAF) 
	return;
	else if (node->alternatives) {
		int i;
		struct tree_node* p;
		massert(children);
		p = children[0]->op==OP_LEAF ? NULL : children[0]->next;
		for(i=0;i<node->alternatives;i++)
			regex_list_dobuild(node,children[i]);
		if(p && p!=node)
			regex_list_dobuild(node,p);
	} else {
		if(children) 
			if (children[0])
				regex_list_dobuild(node,children[0]);
	}
	if(node->next && !node->listend)
		regex_list_dobuild(node,node->next);
	if(node->op==OP_TMP_PARCLOSE)
		node->c=0;
	*free(node);*
}
*/

/* Build the matcher list */
static int build_regex_list(struct regex_matcher* matcher)
{
	int rc;
	if(!matcher->list_inited || !matcher->list_loaded) {
		cli_errmsg("Regex list not loaded!\n");
		return -1;/*TODO: better error code */
	}
	cli_dbgmsg("Building regex list\n");
	if(matcher->root_hosts)
		if(( rc = cli_ac_buildtrie(&matcher->root_hosts[matcher->root_hosts_cnt-1]) ))
 			return rc;
	matcher->list_built=1;

	return CL_SUCCESS;
}

/* Done with this matcher, free resources */
void regex_list_done(struct regex_matcher* matcher)
{
	massert(matcher);

	regex_list_cleanup(matcher);
	if(matcher->list_loaded) {
		if(matcher->root_hosts) {
			size_t i;
			for(i=0;i<matcher->root_hosts_cnt;i++)
				cli_ac_free(&matcher->root_hosts[i]);
			free(matcher->root_hosts);
			matcher->root_hosts=NULL;
		}

		matcher->root_hosts_cnt=0;
		matcher->list_built=0;
		destroy_tree(matcher);
		matcher->list_loaded=0;
	}
	if(matcher->list_inited) {
		matcher->list_inited=0;
	}
	stack_destroy(&matcher->node_stack);
	stack_destroy(&matcher->node_stack_alt);
}

/* Tree matcher algorithm */
struct token_t
{
	union {
		const unsigned char* start;
		char_bitmap_p  bitmap;
		unsigned char  c;
	} u;
	size_t len;
	char   type;
};

enum {TOKEN_CHAR,TOKEN_DOT,TOKEN_PAR_OPEN,TOKEN_PAR_CLOSE,TOKEN_BRACKET,TOKEN_ALT,TOKEN_REGEX,TOKEN_DONE};

static const unsigned char* getNextToken(const unsigned char* pat,struct token_t* token)
{
	massert(pat);
	massert(token);

	switch(*pat) {
		case '\\':
			token->type=TOKEN_CHAR;
			token->u.c = *(++pat);
			if(islower(token->u.c)) {
				/* handle \n, \t, etc. */
				char fmt[3] = {'\\', '\0', '\0'};
				char c;

				fmt[1] = token->u.c;
				if(snprintf(&c,1,fmt)!=1) {
					token->type=TOKEN_REGEX;
					token->u.start = pat;
				}
				else
					token->u.c=c;
			}
			token->len = 1;
			break;
		case '|':
			token->type=TOKEN_ALT;
			break;
		case '*':
		case '+':
		case '?':
		case '{':
		case '}':
			token->type=TOKEN_REGEX;
/*			massert(0 && "find_regex_start should have forbidden us from finding regex special chars");*/
			break;
		case '[':
			{
			/*TODO: implement*/
			/*see if it is something simple like a list of characters, a range, or negated ...*/
			const unsigned char* old=pat++;/* save this in case we change our mind and decide this is too complicated for us to handle*/
			unsigned char range_start=0;
			int hasprev = 0;
			char_bitmap_p bitmap = cli_malloc(32);
			if(!bitmap)
				return NULL;
			if (*pat=='^') {
				memset(bitmap,0xFF,32);/*match chars not in brackets*/
				pat++;
			}
			else
				memset(bitmap,0x00,32);
			do {
				/* literal ] can be first character, so test for it at the end of the loop, for example: []] */
				if (*pat=='-' && hasprev) {
					/* it is a range*/
					unsigned char range_end;
					unsigned int c;
					massert(range_start);
					pat++;
					if (pat[0]=='[')
						if (pat[1]=='.') {
							if(pat[2]=='-' && pat[3]=='.' && pat[4]==']')
								range_end = '-';
							else {
								/* this is getting complicated, bail out */
								cli_warnmsg("confused about collating sequences in regex,bailing out");
								pat=old;
								token->type=TOKEN_REGEX;
								break;
							}
						}
						else 
							range_end = *pat;
					else
						range_end = *pat;
					for(c=range_start+1;c<=range_end;c++)
						bitmap[c>>3] ^= 1<<(c&0x7);
					hasprev = 0;
				}
				else if (pat[0]=='[' && pat[1]==':') {
							const unsigned char* end;
							int len,found=-1;
							size_t i;

							pat+=2;
							end=(unsigned char*)strstr((const char*)pat,":]");
							if(!end) {
								cli_warnmsg("confused about std char class syntax regex,bailing out");
								pat=old;
								token->type=TOKEN_REGEX;
								break;
							}

							len = end-pat;
							for(i=0;i<std_class_cnt;i++)
								if(!strncmp((const char*)pat,std_class[i],len)) {
									found=i;
									break;
								}
							if(found!=-1) {
								for(i=0;i<256;i++)
									if(char_class[i]&(1<<found))
										bitmap[i>>3] ^= 1<<(i&0x7);
							}
							else {
								/*unknown class*/
								cli_warnmsg("confused about regex bracket expression, bailing out");
								pat=old;
								token->type=TOKEN_REGEX;
								break;
							}
						}
				else {
					bitmap[*pat>>3] ^= 1<<(*pat&0x7);
					pat++;
					range_start = *pat;
					hasprev = 1;
				}
			} while(*pat!=']');
			/*TODO: see if this bitmap already exists, then reuse*/			
			token->type = TOKEN_BRACKET;
			token->u.bitmap = bitmap;
			break;
			}
		case ']':
			massert(0 && "Encountered ] without matching [");
			/* bad state */
			break;
		case '.':
			token->type=TOKEN_DOT;
			break;
		case '(':
			token->type=TOKEN_PAR_OPEN;
			break;
		case ')':
			token->type=TOKEN_PAR_CLOSE;
			break;
		default:
			token->type=TOKEN_CHAR;
			token->u.c = *pat;
			token->len=1;
			break;
	}
	return ++pat;
}

#define INITIAL_ALT_STACK 10
#define ALT_STACK_GROW 20

static const unsigned char* find_regex_start(const unsigned char* pat)
{
	struct token_t token;
	/*TODO: find where the regex part begins, for ex:
	 * abcd+, regex begins at 'd'
	 * */
	const unsigned char* last=NULL;
	const unsigned char* tmp=NULL;
	const unsigned char** altpositions = cli_malloc(INITIAL_ALT_STACK*sizeof(*altpositions));
	size_t altpositions_capacity = INITIAL_ALT_STACK;
	size_t altpositions_cnt = 0;
	char lasttype = -1;
	if(!altpositions)
		return NULL;
	massert(pat);

	/* Try to parse pattern till special regex chars are encountered, that the tree-matcher doesn't handle, like: +,*,{}.
	 * The tricky part is that once we encounter these, the previous 'atom' has to be passed on to the regex matcher, so we have to
	 * back up to the last known good position
	 * Example, if we have: abc(defg)+, then only abc can be handled by tree parser, so we have to return the position of (.
	 * Another example: abc(defg|xyz|oz+|pdo), the last known good position is |, after xyz
	 * TODO: what about open parantheses? maybe once we found a special char, we have top back out before the first (?
	 * */
	do {	
		tmp = pat;
		pat = getNextToken(pat,&token);
		if(token.type!=TOKEN_REGEX) {
			last = tmp;
			lasttype = token.type;
			if(token.type==TOKEN_BRACKET && token.u.bitmap)
				free(token.u.bitmap);
			if(token.type==TOKEN_ALT || token.type==TOKEN_PAR_OPEN) {
				/* save this position on stack, succesfully parsed till here*/
				if(altpositions_cnt && altpositions[altpositions_cnt-1][0]=='|')
					/* encountered another alternate (|) operator, override previous | position stored */
					altpositions[altpositions_cnt-1]=last;
				else {
					altpositions[altpositions_cnt++] = last;
					if(altpositions_cnt == altpositions_capacity) {
						altpositions_capacity += ALT_STACK_GROW;
						altpositions = cli_realloc(altpositions,altpositions_capacity*sizeof(*altpositions));
						if(!altpositions)
							return NULL;
					}
				}
			} else if (lasttype==TOKEN_PAR_CLOSE) {
				/* remove last stored position from stack, succesfully this last group */
				altpositions_cnt--;
				massert(altpositions_cnt>0);
			}
		}
		else {
			if(altpositions_cnt)
				last = altpositions[0 /*altpositions_cnt-1*/];/*TODO: which index here?, see above TODO... */
			/*last stored 'safe' position where no special (+,*,{}) regex chars were encountered*/
		}
	} while(*pat && token.type!=TOKEN_REGEX);
	free(altpositions);
	return *pat ? last : last+1;
}

static struct tree_node* tree_node_alloc(struct tree_node* next,char listend)
{
	struct tree_node* node = cli_malloc(sizeof(*node));
	if(node) {
		node->alternatives=0;
		node->next=next;
		node->listend=listend;
		node->u.children=NULL;
	}
	return node;
}

static struct tree_node* tree_root_alloc(void)
{
	struct tree_node* root=tree_node_alloc(NULL,1);
	if(root) {
		root->op=OP_ROOT;
		root->c=0;
		root->next=NULL;
		root->listend=1;
	}
	return root;
}

static inline struct tree_node* tree_node_char_binsearch(const struct tree_node* node,const char csearch,int* left)
{
	int right;
	struct tree_node **children;
	massert(node);
	massert(left);

	children = tree_node_get_children(node);
	right = node->alternatives-1;
	*left = 0;
	if(!node->alternatives)
		return NULL;
	massert(children);
	while(*left<=right) {
		int mid  = *left+(right-*left)/2;
		if(children[mid]->c == csearch)
			return children[mid]; 
		else if(children[mid]->c < csearch)
			*left=mid+1;
		else
			right=mid-1;
	}
	return NULL;
}

static inline struct tree_node* tree_get_next(struct tree_node* node)
{
	struct tree_node** children;
	massert(node);
	children = tree_node_get_children(node);

	if(!node->alternatives && children && children[0])
		return children[0];
	else if(node->alternatives<=1)
		return node;
	else
		return children[0]->next;
}

static inline size_t tree_node_get_array_size(const struct tree_node* node)
{
	massert(node);
	/* if op is CUSTOMCLASS, then first pointer is pointer to bitmap, so array size is +1 */
	return (node->alternatives + (node->op==OP_CUSTOMCLASS ? 1 : 0)) * sizeof(node->u.children[0]);
}

static inline struct tree_node* tree_node_char_insert(struct tree_node* node,const char c,int left)
{
	struct tree_node* new, *alt = tree_get_next(node);
	struct tree_node **children;
	node->alternatives++;
	node->u.children = cli_realloc(node->u.children,tree_node_get_array_size(node));
	if(!node->u.children)
		return NULL;

	children = node->op==OP_CUSTOMCLASS ? node->u.children+1 : node->u.children;

	new = tree_node_alloc(alt , node == alt );
	if(new) {
		new->op=OP_CHAR;
		new->c=c;
	}

	if(node->alternatives-left-1>0)
			memmove(&children[left+1],&children[left],(node->alternatives-left-1)*sizeof(node->u.children[0]));
	children[left] = new;	

	return new;
}

static inline void tree_node_insert_nonbin(struct tree_node* node, struct tree_node* new)
{
	struct tree_node **children;
	massert(node);
	massert(new);

	children = tree_node_get_children(node);
	if(node->alternatives) {
		massert(children);
	       	if(children[0]->next == node) {
			int i;
			new->listend = 1;
			for(i=0;i<node->alternatives;i++) {
				children[i]->next = new;
				children[i]->listend = 0;
			}
		}
		else {
			struct tree_node* p;
			for(p = children[0]->next ; p->next != node ; p = p->next)
				massert(!p->listend);
			new->listend = 1;
			p->listend = 0;
			p->next = new;
		}
	}
	else {
		int idx = node->op==OP_CUSTOMCLASS ? 1 : 0;
		if(node->u.children)
			if(node->u.children[idx]) {
				node = node->u.children[idx];
				while(node->next && !node->listend)
					node = node->next;
				node->listend = 0;
				node->next = new;
				new->listend=1;
			}
		node->u.children = cli_realloc(node->u.children,sizeof(node->u.children[0])*(2));
		if(node->u.children) {
			node->u.children[idx] = new;
		}
	}
}

static inline unsigned char char_getclass(const unsigned char* bitmap)
{
	size_t i;
	massert(bitmap);

	for(i=0;i<std_class_cnt;i++)
		if(!memcmp(bitmap,char_class_bitmap[i],256>>3))
			return i;
	return std_class_cnt;
}

static void stack_destroy(struct node_stack* stack)
{
	massert(stack);
	if(stack->data)
		free(stack->data);
	stack->data = NULL;
	stack->capacity = 0;
}

/* call this after whitelist load is complete, and the tree is no longer going to be modified */
void regex_list_cleanup(struct regex_matcher* matcher)
{
	massert(matcher);

	stack_destroy(&matcher->node_stack);
	stack_destroy(&matcher->node_stack_alt);
	stack_init(&matcher->node_stack);
	stack_init(&matcher->node_stack_alt);
}

int is_regex_ok(struct regex_matcher* matcher)
{
	massert(matcher);
	return (!matcher->list_inited || matcher->list_inited!=-1);/* either we don't have a regexlist, or we initialized it successfully */
}

/* returns 0 on success, regexec error code otherwise */						
static int add_pattern(struct regex_matcher* matcher,const unsigned char* pat,const char* info)
{
	int bol=1;
	const unsigned char* pat_end = find_regex_start(pat);
	struct token_t token;
	struct tree_node* node;
	
	massert(matcher);

	node = matcher->root_regex;

	stack_reset(&matcher->node_stack);
	stack_reset(&matcher->node_stack_alt);
	stack_push(&matcher->node_stack,node);

	for(;node->op!=OP_LEAF;){
		if(pat<pat_end)
			pat  = getNextToken(pat,&token);
		else if(*pat) {
			token.type = TOKEN_REGEX;
			token.u.start=pat;
		}
		else
			token.type = TOKEN_DONE;

		switch(token.type) {
			case TOKEN_CHAR: 
				{
					/* search for char in tree */
					int left;
					struct tree_node* newnode = tree_node_char_binsearch(node,token.u.c,&left);
					if(newnode)
						node = newnode;
					else {
						/* not found, insert it */
						node = tree_node_char_insert(node,token.u.c,left);
					}
					break;
				}

			case TOKEN_PAR_OPEN:
				stack_push(&matcher->node_stack_alt,NULL);/* marker */
				stack_push(&matcher->node_stack,node);
				break;

			case TOKEN_PAR_CLOSE: {
						      /*TODO: test this!!!*/
						      struct tree_node* node_alt = node;
						      node = tree_node_alloc(NULL,1);
						      node->op=OP_PARCLOSE;
						      node->c=0;
						      node->listend=1;
						      tree_node_insert_nonbin(node_alt,node);
						      while (( node_alt = stack_pop(&matcher->node_stack_alt) )) {
							      tree_node_insert_nonbin(node_alt,node);
						      }
				      		      stack_pop(&matcher->node_stack);					      
		      				      break;
					      }

			case TOKEN_ALT:
				stack_push(&matcher->node_stack_alt,node);
				node = stack_pop(&matcher->node_stack);
				stack_push(&matcher->node_stack,node);
				break;

			case TOKEN_BRACKET:
				{
					struct tree_node* new = tree_node_alloc(tree_get_next(node),1);
					unsigned char charclass = char_getclass(token.u.bitmap);
					if(charclass == std_class_cnt) {/*not a std char class*/
						new->op = OP_CUSTOMCLASS;
						new->u.children = cli_malloc(sizeof(new->u.children[0])*2);
						if(!new->u.children)
							return CL_EMEM;
						new->u.bitmap[0] = token.u.bitmap;
						new->u.bitmap[1] = NULL;
						tree_node_insert_nonbin(node,new);
						node = new;
					}
					else {
						new->op = OP_STDCLASS;
						new->c = charclass;
						tree_node_insert_nonbin(node,new);
						node=new;
					}
					break;
				}

			case TOKEN_DOT:
				{
					struct tree_node* new = tree_node_alloc(tree_get_next(node),1);
					new->op = OP_DOT;
					tree_node_insert_nonbin(node,new);
					node=new;
					break;
				}

			case TOKEN_REGEX:
			case TOKEN_DONE: {
						 struct leaf_info* leaf=cli_malloc(sizeof(*leaf));
						 if(!leaf)
							 return CL_EMEM;
						 leaf->info=strdup(info);
						 if(token.type==TOKEN_REGEX) {
							 int rc;
							 struct tree_node* new;
							 regex_t* preg;
							 preg=cli_malloc(sizeof(*preg));
							 if(!preg)
								 return CL_EMEM;
							 rc = regcomp(preg,(const char*)token.u.start,REG_EXTENDED|(bol?0:REG_NOTBOL));
							 leaf->preg=preg;
							 if(rc)
								 return rc;
							 new=cli_malloc(sizeof(*new));
							 if(!new)
								 return CL_EMEM;
							 new->op=OP_LEAF;
							 new->next=node;
							 new->alternatives=0;
							 new->u.leaf=leaf;
							 new->listend=1;
							 tree_node_insert_nonbin(node,new);
						 }
						 else {
							 leaf->preg=NULL;
							 node->alternatives=0;
							 node->u.leaf=leaf;
							 node->op=OP_LEAF;
						 }
						 return 0;
					 }
		}

		bol=0;
	}
	return 0;
}

/* c has to be unsigned char here!! */
static int match_node(struct tree_node* node,const unsigned char* c,size_t len,const char** info)
{
	struct tree_node** children;
	int rc;

	massert(node);
	massert(c);
	massert(info);

	if(!node->u.children)
		return MATCH_FAILED;/* tree empty */
	*info = NULL;
	len++;
	c--;
	for(;;) {
		massert(node);
		children = node->u.children;
		switch(node->op) {
			case OP_ROOT:
				rc=1;
				break;
			case OP_PARCLOSE:
				/*this isn't a real character, so don't move*/
				c--;
				len++;
				rc=1;
				break;
			case OP_CHAR:
				massert(*c==node->c && "We know this has to match");
				rc = 1;/* *c==node->c;- we know it has matched */
				break;
			case OP_DOT:	
				rc = *c!='\n';
				break;
			case OP_STDCLASS:
				rc = char_class[*c]&(node->c);
				break;
			case OP_CUSTOMCLASS:
			{
				char_bitmap_p bitmap;
				massert(children);
				bitmap = (char_bitmap_p)node->u.bitmap[0];
				children++;
				rc = bitmap[*c>>3]&(1<<(*c&0x7));
				break;
			}
			case OP_LEAF:
			{
				const struct leaf_info* leaf = node->u.leaf;
				/*isleaf = 1;*/
				if(leaf->preg) {
					rc = !regexec(leaf->preg,(const char*)c,0,NULL,0);
				}
				else  {
					massert(*c==node->c && "We know this has to match[2]");
					rc = 1;
				}
				if(rc) {
					*info = leaf->info;
					return MATCH_SUCCESS;
				}
				break;
			}
			default:
				/* impossible */
				cli_errmsg("Encountered invalid operator in tree:%d\n",node->op);
				exit(1);
		}
		len--;
		if(!len) rc=0;
		c++;
		if(rc) {
			const char csearch = *c;
			int left = 0,right = node->alternatives-1;
			int mid;
			/*matched so far, go deeper*/
			/*do a binary search between children */
			massert(children);
			while(left<=right) {
				mid  = left+(right-left)/2;
				if (children[mid]->c == csearch)
					break;
				else if(children[mid]->c < csearch)
					left=mid+1;
				else
					right=mid-1;
			}
			if(left<=right) {
				node = children[mid];
				massert(node);
			}
			else {
				if(node->alternatives) {
					if(!children[0]->listend) {
						node = children[0];
						c++;
						len--;
					}
					while(node && node->listend) {
						node = node->next;/* climb up */
						c--;
						len++;
					}
					if(!node || !node->next) 
						return MATCH_FAILED;/* reached root node */
					node=node->next;
					c--;
					len++;
				}
				else if(node->u.children) {
					struct tree_node* rewrite_next = NULL;
					if(node->op==OP_PARCLOSE) 
						rewrite_next = node;
					node = children[0];
					massert(node);
					massert(node->op!=OP_CHAR);
					if(rewrite_next)
						node->next = rewrite_next;/* this node is pointed to by several parent nodes, 
									     we need to know 
									     from which one we came, so we can find out way back
									     should we fail to match somewhere deeper*/
				}
			}
		}
		else {
			/* this node didn't match, try sibling, or parent (if no more siblings) */
			while(node && node->listend) {
				node = node->next;/* sibling of parent */
				c--;
				len++;
			}
			if(!node || !node->next) /* reached root node, it has no next */
				return MATCH_FAILED;
			else {
				c--;
				len++;
				node=node->next;
			}
		}
	}
	return MATCH_FAILED;
}

/* push node on stack, only if it isn't there already */
static inline void stack_push_once(struct node_stack* stack,struct tree_node* node)
{
	size_t i;
	massert(stack);
	massert(node);

	for(i=0;i < stack->cnt;i++)
		if(stack->data[i]==node)
			return;
	stack_push(stack,node);
}

static void destroy_tree_internal(struct regex_matcher* matcher,struct tree_node* node)
{
	struct tree_node **children;
	massert(matcher);
	massert(node);

	children = tree_node_get_children(node);
	if(node->op==OP_LEAF) {
		struct leaf_info* leaf = node->u.leaf;
		if(node->next && !node->listend)
			destroy_tree_internal(matcher,node->next);
		stack_push_once(&matcher->node_stack,(struct tree_node*)node->u.leaf);/* cast to make compiler happy, and to not make another stack implementation for storing void* */
		stack_push_once(&matcher->node_stack,node);
		if(leaf->preg) {
			regfree(leaf->preg);
			free(leaf->preg);
			leaf->preg=NULL;
		}
		if(leaf->info) {
			free(leaf->info);
			leaf->info=NULL;
		}
	/*	return;*/
	}
	if(node->alternatives) {
		int i;
		struct tree_node* p;
		massert(children);
		p = children[0]->op==OP_LEAF ? NULL : children[0]->next;
		for(i=0;i<node->alternatives;i++)
			destroy_tree_internal(matcher,children[i]);
		if(p && p!=node)
			destroy_tree_internal(matcher,p);/*?? is this ok, or without _internal?*/
	}
	else {
		if(children) {
			if(children[0])
				destroy_tree_internal(matcher,children[0]);		
		}
	}
	if(node->op!=OP_LEAF && node->next && !node->listend)
		destroy_tree_internal(matcher,node->next);
	if(node->u.children)
		stack_push_once(&matcher->node_stack,(struct tree_node*)node->u.children);/* cast to make compiler happy, it isn't really a tree_node* */
	if(node->op==OP_CUSTOMCLASS && node->u.children[0]) {
		free(node->u.children[0]);
		node->u.children[0]=NULL;
	}
	stack_push_once(&matcher->node_stack,node);
}

static void destroy_tree(struct regex_matcher* matcher)
{
	/* we might have the same node linked by different nodes, so a recursive walk&free doesn't work in all situations,
	 * i.e. it might double-free, so instead of freeing, just push the nodes on a stack, and later free the nodes in that stack,
	 * (and push to stack only if it doesn't contain it already*/
	massert(matcher);

	stack_reset(&matcher->node_stack);
	destroy_tree_internal(matcher,matcher->root_regex);
	while (matcher->node_stack.cnt) {
		struct tree_node* node = stack_pop(&matcher->node_stack);
		if(node)
			free(node);
	}
}
#ifndef NDEBUG
static void dump_node(struct tree_node* node)
{
	int i;
	struct tree_node* p,**children;
	massert(node);
	if(node->op==OP_LEAF) {
		if(node->u.leaf->preg)
			printf("n%p [label=\"regex\\nleaf\"]",(void*)node);
		else
			printf("n%p [label=\"%c\\nleaf\"];\n",(void*)node,node->c);
		if(node->next && !node->listend) {
			printf("n%p -> n%p;\n",(void*)node,(void*)node->next);
			dump_node(node->next);
		}
		return;
	}
	printf("n%p [label=\"%c\\n%d\\nlistend:%d\"];\n",(void*)node,(node->op==OP_ROOT||node->op==OP_PARCLOSE) ?'@' :node->c,node->op,node->listend);
	if(node->next)
		printf("n%p -> n%p;\n",(void*)node,(void*)node->next);
	printf("n%p -> {",(void*)node);/*using address of node as id*/
	children = tree_node_get_children(node);
	if(node->alternatives)
		massert(children);
	for(i=0;i<node->alternatives;i++)
		printf("n%p ",(void*)children[i]);
	if(node->alternatives && children[0]->op!=OP_LEAF)
		for(p=children[0]->next;p!=node;p=p->next)
		{
			massert(p);
			printf("n%p ",(void*)p);
			if(p->op==OP_LEAF || p->listend)
				break;
		}
	if(!node->alternatives && children && children[0])
		printf("n%p ",(void*)children[0]);
	printf("};\n");
	printf("{rank=same;");
	for(i=0;i<node->alternatives;i++)
		printf("n%p ",(void*)node->u.children[i]);
	if(node->alternatives && children[0]->op!=OP_LEAF)
		for(p=children[0]->next;p!=node;p=p->next) 
		{
			printf("n%p ",(void*)p);	
			if(p->op==OP_LEAF || p->listend)
				break;
		}
	if(!node->alternatives && children && children[0])
		printf("n%p ",(void*)children[0]);
	printf("};\n");
	for(i=0;i<node->alternatives;i++)
		dump_node(children[i]);
	if(node->alternatives && children[0]->op!=OP_LEAF)
		for(p=children[0]->next;p!=node;p=p->next)
		{
			dump_node(p);
			if(p->op==OP_LEAF || p->listend)
				break;
		}
	if(!node->alternatives && children && children[0])
		dump_node(children[0]);
}

void dump_tree(struct tree_node* root)
{
	/*use dot/dotty from graphviz to view it*/
	massert(root);
	printf("digraph tree {\n");
	dump_node(root);
	printf("}\n");
}
#endif

#endif
