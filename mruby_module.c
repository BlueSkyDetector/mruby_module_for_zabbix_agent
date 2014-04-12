/*
** Takanori Suzuki <mail.tks@gmail.com>
** Copyright (C) 2001-2014
**
** derived from Zabbix SIA's work under GPL2 or later.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
** MA 02110-1301, USA.
**/

#include <sysinc.h>
#include <module.h>
#include <common.h>
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/string.h>

extern char *CONFIG_LOAD_MODULE_PATH;

typedef struct
{
	char		*path;
	mrb_state	*mrb;
}
MRB_WITH_PATH;

MRB_WITH_PATH	*mrb_list = NULL;
int		mrb_list_len = 0;

/* the variable keeps timeout setting for item processing */
static int item_timeout = 0;

int zbx_module_mruby_file(AGENT_REQUEST *request, AGENT_RESULT *result);
int zbx_module_mruby_string(AGENT_REQUEST *request, AGENT_RESULT *result);

static ZBX_METRIC keys[] =
/* KEY FLAG FUNCTION TEST PARAMETERS */
{
	{"mruby.file", CF_HAVEPARAMS, zbx_module_mruby_file, NULL},
	{"mruby.string", CF_HAVEPARAMS, zbx_module_mruby_string, NULL},
	{NULL}
};

/******************************************************************************
*                                                                             *
* Function: zbx_module_api_version                                            *
*                                                                             *
* Purpose: returns version number of the module interface                     *
*                                                                             *
* Return value: ZBX_MODULE_API_VERSION_ONE - the only version supported by    *
* Zabbix currently                                                            *
*                                                                             *
******************************************************************************/
int zbx_module_api_version()
{
	return ZBX_MODULE_API_VERSION_ONE;
}

/******************************************************************************
*                                                                             *
* Function: zbx_module_item_timeout                                           *
*                                                                             *
* Purpose: set timeout value for processing of items                          *
*                                                                             *
* Parameters: timeout - timeout in seconds, 0 - no timeout set                *
*                                                                             *
******************************************************************************/
void zbx_module_item_timeout(int timeout)
{
	item_timeout = timeout;
}

/******************************************************************************
*                                                                             *
* Function: zbx_module_item_list                                              *
*                                                                             *
* Purpose: returns list of item keys supported by the module                  *
*                                                                             *
* Return value: list of item keys                                             *
*                                                                             *
******************************************************************************/
ZBX_METRIC *zbx_module_item_list()
{
	return keys;
}

/******************************************************************************
*                                                                             *
* Function: zbx_module_mruby_file                                             *
*                                                                             *
* Purpose: function for "mruby.file" key                                      *
*                                                                             *
* Return value: SYSINFO_RET_OK                                                *
*                                                                             *
******************************************************************************/
int zbx_module_mruby_file(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	char		*param1 = NULL;
	mrb_value	ret;
	char		mruby_file_path[MAX_BUFFER_LEN];
	int		i = 0;
	int		find = 0;

	param1 = get_rparam(request, 0);
	zbx_snprintf(mruby_file_path, sizeof(mruby_file_path), "%s/%s/%s", CONFIG_LOAD_MODULE_PATH, "mruby", param1);

	for (i = 0; i < mrb_list_len; i++)
	{
		if (!strcmp(mruby_file_path, mrb_list[i].path))
		{
			find = 1;
			break;
		}
	}

	if (find)
	{
		ret = mrb_funcall(mrb_list[i].mrb, mrb_top_self(mrb_list[i].mrb), "zbx_module_run", 0);
		/* ret should be zabbix compatible type, like string, int, float  */
		if (mrb_list[i].mrb->exc)
			SET_TEXT_RESULT(result, strdup(""));
		else if (MRB_TT_STRING == ret.tt)
			SET_TEXT_RESULT(result, strdup(RSTRING_PTR(ret)));
		else if (MRB_TT_FLOAT == ret.tt)
			SET_DBL_RESULT(result, mrb_float(ret));
		else if (MRB_TT_FIXNUM == ret.tt)
			SET_DBL_RESULT(result, mrb_fixnum(ret));
		else
			SET_TEXT_RESULT(result, strdup(""));
	}
	else
		SET_TEXT_RESULT(result, strdup(""));

	return SYSINFO_RET_OK;
}

/******************************************************************************
*                                                                             *
* Function: zbx_module_mruby_string                                           *
*                                                                             *
* Purpose: function for "mruby.string" key                                    *
*                                                                             *
* Return value: SYSINFO_RET_OK                                                *
*                                                                             *
******************************************************************************/
int zbx_module_mruby_string(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	char		*param1 = NULL;
	mrb_state	*mrb = mrb_open();
	mrb_value	ret;

	param1 = get_rparam(request, 0);
	ret = mrb_load_string(mrb, param1);
	/* ret should be zabbix compatible type, like string, int, float  */
	if (mrb->exc)
		SET_TEXT_RESULT(result, strdup(""));
	else if (MRB_TT_STRING == ret.tt)
		SET_TEXT_RESULT(result, strdup(RSTRING_PTR(ret)));
	else if (MRB_TT_FLOAT == ret.tt)
		SET_DBL_RESULT(result, mrb_float(ret));
	else if (MRB_TT_FIXNUM == ret.tt)
		SET_DBL_RESULT(result, mrb_fixnum(ret));
	else
		SET_TEXT_RESULT(result, strdup(""));
	mrb_close(mrb);

	return SYSINFO_RET_OK;
}

/******************************************************************************
*                                                                             *
* Function: search_and_load_mruby_files                                       *
*                                                                             *
* Purpose: load "*.rb" files in CONFIG_LOAD_MODULE_PATH + "/mruby"            *
*                                                                             *
******************************************************************************/
void search_and_load_mruby_files()
{
	char mruby_dir_path[MAX_BUFFER_LEN];
	char		mruby_file_path[MAX_BUFFER_LEN];
	struct dirent	*dir_entry = NULL;
	DIR		*dir = NULL;
	FILE		*f = NULL;
	int 		i = 0;

	zbx_snprintf(mruby_dir_path, sizeof(mruby_dir_path), "%s/%s", CONFIG_LOAD_MODULE_PATH, "mruby");

	if (NULL == (dir = opendir(mruby_dir_path)))
		return;

	for (i = 0; NULL != (dir_entry = readdir(dir)); i++){
		if (!strcmp(&(dir_entry->d_name[strlen(dir_entry->d_name)-3]),".rb"))
		{
			mrb_list_len++;
			if (NULL == mrb_list)
				mrb_list = zbx_malloc(mrb_list, sizeof(mrb_list) * mrb_list_len);
			else
				mrb_list = zbx_realloc(mrb_list, sizeof(mrb_list) * mrb_list_len);

			zbx_snprintf(mruby_file_path, sizeof(mruby_file_path), "%s/%s", mruby_dir_path, dir_entry->d_name);
			mrb_list[mrb_list_len - 1].path = zbx_strdup(NULL, mruby_file_path);
			mrb_list[mrb_list_len - 1].mrb = mrb_open();
			f = fopen(mruby_file_path, "r");
			if(f)
			{
				mrb_load_file(mrb_list[mrb_list_len - 1].mrb, f);
				fclose(f);
			}
		}
	}
	closedir(dir);
	return;
}

/******************************************************************************
*                                                                             *
* Function: exec_mruby_function                                               *
*                                                                             *
* Purpose: execute "function" in loaded "*.rb" files                          *
*                                                                             *
******************************************************************************/
void exec_mruby_function(char *function)
{
	int i;

	for (i = 0; i < mrb_list_len; i++)
	{
		mrb_funcall(mrb_list[i].mrb, mrb_top_self(mrb_list[i].mrb), function, 0);
	}

	return;
}

/******************************************************************************
*                                                                             *
* Function: zbx_module_init                                                   *
*                                                                             *
* Purpose: the function is called on agent startup                            *
* It should be used to call any initialization routines                       *
*                                                                             *
* Return value: ZBX_MODULE_OK - success                                       *
* ZBX_MODULE_FAIL - module initialization failed                              *
*                                                                             *
* Comment: the module won't be loaded in case of ZBX_MODULE_FAIL              *
*                                                                             *
******************************************************************************/
int zbx_module_init()
{
	search_and_load_mruby_files();
	exec_mruby_function("zbx_module_init");
	return ZBX_MODULE_OK;
}

/******************************************************************************
*                                                                             *
* Function: zbx_module_uninit                                                 *
*                                                                             *
* Purpose: the function is called on agent shutdown                           *
* It should be used to cleanup used resources if there are any                *
*                                                                             *
* Return value: ZBX_MODULE_OK - success                                       *
* ZBX_MODULE_FAIL - function failed                                           *
*                                                                             *
******************************************************************************/
int zbx_module_uninit()
{
	int i;

	exec_mruby_function("zbx_module_uninit");

	for(i = 0; i < mrb_list_len; i++)
	{
		zbx_free(mrb_list[i].path);
		mrb_close(mrb_list[i].mrb);
	}
	zbx_free(mrb_list);

	return ZBX_MODULE_OK;
}
