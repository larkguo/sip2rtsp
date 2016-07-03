/***************************************************************************
*            cfg.c
*
*  Thu Mar 10 11:13:44 2005
*  Copyright  2005  Simon Morlat
*  Email simon.morlat@linphone.org
****************************************************************************/

/*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU Library General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef WIN32
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif 
#include "cfg.h"

/*----------------------------------------*/
CfgList *cfg_list_new(void *data)
{
	CfgList *new_elem=(CfgList *)calloc(sizeof(CfgList),1);
	new_elem->prev=new_elem->next=NULL;
	new_elem->data=data;
	return new_elem;
}

CfgList *cfg_list_remove_link(CfgList *list, CfgList *elem)
{
	CfgList *ret;
	if (elem==list)
	{
		ret=elem->next;
		elem->prev=NULL;
		elem->next=NULL;
		if (ret!=NULL) ret->prev=NULL;
		if( NULL != elem)
		{
			free(elem);
			elem = NULL;
		}
		return ret;
	}
	elem->prev->next=elem->next;
	if (elem->next!=NULL) 
	{
		elem->next->prev=elem->prev;
	}
	elem->next=NULL;
	elem->prev=NULL;
	if( NULL != elem)
	{
		free(elem);
		elem = NULL;
	}
	return list;
}

CfgList *cfg_list_find(CfgList *list, void *data)
{
	for(;list!=NULL;list=list->next)
	{
		if (list->data==data) 
		{
			return list;
		}
	}
	return NULL;
}

CfgList * cfg_list_append(CfgList *elem, void * data)
{
	CfgList *new_elem=cfg_list_new(data);
	CfgList *it=elem;
	if (elem==NULL) 
	{
		return new_elem;
	}
	while (it->next!=NULL) 
	{
		it=it->next;
	}
	it->next=new_elem;
	new_elem->prev=it;
	return elem;
}

CfgList * cfg_list_prepend(CfgList *elem, void *data)
{
	CfgList *new_elem=cfg_list_new(data);
	if (elem!=NULL) 
	{
		new_elem->next=elem;
		elem->prev=new_elem;
	}
	return new_elem;
}

CfgList * cfg_list_concat(CfgList *first, CfgList *second)
{
	CfgList *it=first;
	if (it==NULL) 
	{
		return second;
	}
	while(it->next!=NULL) 
	{
		it=it->next;
	}
	it->next=second;
	second->prev=it;
	return first;
}

CfgList * cfg_list_free(CfgList *list)
{
	CfgList *elem = list;
	CfgList *tmp;
	if (list==NULL) return NULL;
	while(elem->next!=NULL) 
	{
		tmp = elem;
		elem = elem->next;
		if( NULL != tmp)
		{
			free(tmp);
			tmp = NULL;
		}
	}
	if( NULL != elem)
	{
		free(elem);
		elem = NULL;
	}
	return NULL;
}

CfgList * cfg_list_remove(CfgList *first, void *data)
{
	CfgList *it;
	it=cfg_list_find(first,data);
	if (it) 
	{
		return cfg_list_remove_link(first,it);
	}
	else 
	{
		printf("cfg_list_remove: no element with %p data was in the list\n", data);
		return first;
	}
}

int cfg_list_size(const CfgList *first)
{
	int n=0;
	while(first!=NULL)
	{
		++n;
		first=first->next;
	}
	return n;
}

void cfg_list_for_each(const CfgList *list, void (*func)(void *))
{
	for(;list!=NULL;list=list->next)
	{
		func(list->data);
	}
}

void cfg_list_for_each2(const CfgList *list, void (*func)(void *, void *), void *user_data)
{
	for(;list!=NULL;list=list->next)
	{
		func(list->data,user_data);
	}
}


CfgList *cfg_list_find_custom(CfgList *list, int (*compare_func)(const void *, const void*), const void *user_data)
{
	for(;list!=NULL;list=list->next)
	{
		if (compare_func(list->data,user_data)==0) 
		{
			return list;
		}
	}
	return NULL;
}

void * cfg_list_nth_data(const CfgList *list, int index)
{
	int i;
	for(i=0;list!=NULL;list=list->next,++i)
	{
		if (i==index) 
		{
			return list->data;
		}
	}
	printf("cfg_list_nth_data: no such index in list.\n");
	return NULL;
}

int cfg_list_position(const CfgList *list, CfgList *elem)
{
	int i;
	for(i=0;list!=NULL;list=list->next,++i)
	{
		if (elem==list) 
		{
			return i;
		}
	}
	printf("cfg_list_position: no such element in list.\n");
	return -1;
}

int cfg_list_index(const CfgList *list, void *data)
{
	int i;
	for(i=0;list!=NULL;list=list->next,++i)
	{
		if (data==list->data) 
		{
			return i;
		}
	}
	printf("cfg_list_index: no such element in list.\n");
	return -1;
}

CfgList *cfg_list_insert_sorted(CfgList *list, void *data, int (*compare_func)(const void *, const void*))
{
	CfgList *it=NULL,*previt=NULL;
	CfgList *nelem=NULL;
	CfgList *ret=list;
	if (list==NULL) 
	{
		return cfg_list_append(list,data);
	}else
	{
		nelem=cfg_list_new(data);
		for(it=list;it!=NULL;it=it->next)
		{
			previt=it;
			if (compare_func(data,it->data)<=0)
			{
				nelem->prev=it->prev;
				nelem->next=it;
				if (it->prev!=NULL)
				{
					it->prev->next=nelem;
				}else
				{
					ret=nelem;
				}
				it->prev=nelem;
				return ret;
			}
		}
		previt->next=nelem;
		nelem->prev=previt;
	}
	return ret;
}

CfgList *cfg_list_insert(CfgList *list, CfgList *before, void *data)
{
	CfgList *elem=NULL;
	if (list==NULL || before==NULL) 
	{
		return cfg_list_append(list,data);
	}
	for(elem=list;elem!=NULL; elem=elem->next)
	{
		if (elem==before)
		{
			if (elem->prev==NULL)
			{
				return cfg_list_prepend(list,data);
			}
			else
			{
				CfgList *nelem=cfg_list_new(data);
				nelem->prev=elem->prev;
				nelem->next=elem;
				elem->prev->next=nelem;
				elem->prev=nelem;
			}
		}
	}
	return list;
}

CfgList *cfg_list_copy(const CfgList *list)
{
	CfgList *copy=NULL;
	const CfgList *iter;
	for(iter=list;iter!=NULL; iter=iter->next)
	{
		copy=cfg_list_append(copy,iter->data);
	}
	return copy;
}


CfgItem * cfg_item_new(const char *key, const char *value)
{
	CfgItem *item=(CfgItem *)calloc(sizeof(CfgItem),1);
	item->key=strdup(key);
	item->value=strdup(value);
	return item;
}

CfgSection *cfg_section_new(const char *name)
{
	CfgSection *sec=(CfgSection *)calloc(sizeof(CfgSection),1);
	sec->name=strdup(name);
	return sec;
}

void cfg_item_destroy(void *pitem)
{
	CfgItem *item=(CfgItem*)pitem;
	if(NULL != item->key)
	{
		free(item->key);
		item->key = NULL;
	}
	if( NULL != item->value)
	{
		free(item->value);
		item->value = NULL;
	}
	if(NULL != item)
	{
		free(item);
		item = NULL;
	}
}

void cfg_section_destroy(CfgSection *sec)
{
	if(NULL== sec)
	{
		return ;
	}
	if( NULL != sec->name)
	{
		free(sec->name);
		sec->name = NULL;
	}
	cfg_list_for_each(sec->items, cfg_item_destroy);
	cfg_list_free(sec->items);
	free(sec);
}

void cfg_section_add_item(CfgSection *sec,CfgItem *item)
{
	sec->items=cfg_list_append(sec->items,(void *)item);
}

void cfg_add_section(Cfg *cfg, CfgSection *section)
{
	cfg->sections=cfg_list_append(cfg->sections,(void *)section);
}

void cfg_remove_section(Cfg *cfg, CfgSection *section)
{
	cfg->sections=cfg_list_remove(cfg->sections,(void *)section);
	cfg_section_destroy(section);
}

static int is_first_char(const char *start, const char *pos)
{
	const char *p=NULL;
	for(p=start;p<pos;p++)
	{
		if (*p!=' ') 
		{
			return 0;
		}
	}
	return 1;
}

CfgSection *cfg_find_section(Cfg *cfg, const char *name)
{
	CfgSection *sec = NULL;
	CfgList *elem = NULL;

	if( NULL == cfg)	 return NULL;
	/*printf("Looking for section %s\n",name);*/
	for (elem=cfg->sections;elem!=NULL; elem=elem->next)
	{
		sec=(CfgSection*)elem->data;
		if (strcmp(sec->name,name)==0)
		{
			/*printf("Section %s found\n",name);*/
			return sec;
		}
	}
	return NULL;
}

CfgItem *cfg_section_find_item(CfgSection *sec, const char *name)
{
	CfgList *elem = NULL;
	CfgItem *item = NULL;

	if( NULL == sec) 	return NULL;
	
	/*printf("Looking for item %s\n",name);*/
	for (elem=sec->items;elem!=NULL;elem=elem->next)
	{
		item=(CfgItem*)elem->data;
		if (strcmp(item->key,name)==0) 
		{
			/*printf("Item %s found\n",name);*/
			return item;
		}
	}
	return NULL;
}

void cfg_parse(Cfg *cfg, FILE *file, const char separator)
{
	char tmp[MAX_LEN]= {'\0'};
	CfgSection *cur=NULL;

	if (file==NULL) 
	{
		return;
	}
#if 0
	cur=cfg_section_new(SECTION_GLOBAL);
	cfg_add_section(cfg,cur);
#endif
	while(fgets(tmp,MAX_LEN,file)!=NULL)
	{
		//tmp[sizeof(tmp) -1] = '\0';
		char *pos1,*pos2;
		tmp[sizeof(tmp) -1] = '\0';

		pos1=strchr(tmp,'[');
		if (pos1!=NULL && is_first_char(tmp,pos1) )
		{
			pos2=strchr(pos1,']');
			if (pos2!=NULL)
			{
				int nbs;
				char secname[MAX_LEN]={0};
				/* found section */
				*pos2='\0';
				nbs = sscanf(pos1+1,"%s",secname);
				if (nbs == 1 )
				{
					if (strlen(secname)>0)
					{
						//str_trim(secname);
						cur=cfg_find_section (cfg,secname);
						if (cur==NULL)
						{
							cur=cfg_section_new(secname);
							cfg_add_section(cfg,cur);
						}
					}
				}else
				{
					printf("parse error!\n");
				}
			}
		}else {
			pos1=strchr(tmp,separator);
			if (pos1!=NULL)
			{
				char key[MAX_LEN]={0};
				*pos1='\0';

				strncpy(key,tmp,sizeof(key)-1);
				//str_trim(key);
				if (sscanf(tmp,"%s",key)>0)
				{
					pos1++;
					pos2=strchr(pos1,'\r');
					if (pos2==NULL)
					{
						pos2=strchr(pos1,'\n');
					}
					if (pos2==NULL) 
					{
						pos2=pos1+strlen(pos1);
					}else 
					{
						*pos2='\0'; /*replace the '\n' */
						pos2--;
					}
					/* remove ending white spaces */
					for (; pos2>pos1 && *pos2==' ';pos2--) 
					{
						*pos2='\0';
					}
					//if (pos2-pos1>=0)
					{
						/* found a pair key,value */
						if (cur!=NULL)
						{
							CfgItem *item=cfg_section_find_item(cur,key);
							if (item==NULL)
							{
								cfg_section_add_item(cur, cfg_item_new(key,pos1));
							}else
							{
								if( NULL != item->value)
								{
									free(item->value);
									item->value = NULL;
								}
								item->value=strdup(pos1);
							}
							/*printf("Found %s %s=%s\n",cur->name,key,pos1);*/
						}else{
							printf("found key,item but no sections\n");
						}
					}
				}
			}
		}
	}
}

Cfg * cfg_new(const char *filename,const char separator)
{
	Cfg *cfg=(Cfg *)calloc(sizeof(Cfg),1);
	if (filename!=NULL){
		cfg->filename=strdup(filename);
		cfg->file=fopen(filename,"r");
		//cfg->file=fopen(filename,"rw");
		if (cfg->file!=NULL)	{
			cfg_parse(cfg,cfg->file,separator);
			fclose(cfg->file);			
#ifndef WIN32
			/* make existing configuration files non-group/world-accessible */
			if (chmod(filename, S_IRUSR | S_IWUSR) == -1)
				printf("unable to correct permissions on configuration file: %s\n",strerror(errno));
#endif 
			cfg->file=NULL;
			cfg->modified=0;
		}
	}
	return cfg;
}

int cfg_read_file(Cfg *cfg, const char *filename, const char separator)
{
	FILE* f=fopen(filename,"r");
	if (f!=NULL)	{
		cfg_parse(cfg,f,separator);
		fclose(f);
		return 0;
	}
	printf("Fail to open file %s\n",filename);
	return -1;
}

void cfg_item_set_value(CfgItem *item, const char *value)
{
	if( NULL == item)	{
		return ;
	}
	if(NULL!= item->value){
		free(item->value);
		item->value = NULL;
	}
	item->value=strdup(value);
}


void cfg_destroy(Cfg *cfg)
{
	if( NULL == cfg){
		return ;
	}
	if (cfg->filename!=NULL) {
		free(cfg->filename);
		cfg->filename = NULL;
	}
	cfg_list_for_each(cfg->sections,(void (*)(void*))cfg_section_destroy);
	cfg_list_free(cfg->sections);
	
	free(cfg);
}

void cfg_section_remove_item(CfgSection *sec, CfgItem *item)
{
	sec->items=cfg_list_remove(sec->items,(void *)item);
	cfg_item_destroy(item);
}

char *cfg_get_string(Cfg *cfg, const char *section, const char *key, char *default_string)
{
	CfgSection *sec=NULL;
	CfgItem *item=NULL;
	sec=cfg_find_section(cfg,section);
	if (sec!=NULL){
		item=cfg_section_find_item(sec,key);
		if (item!=NULL) 	{
			return item->value;
		}
	}
	return default_string;
}

int cfg_get_int(Cfg *cfg,const char *section, const char *key, int default_value)
{
	const char *str=cfg_get_string(cfg,section,key,NULL);
	if (str!=NULL) {
		return atoi(str);
	}else {
		return default_value;
	}
}

float cfg_get_float(Cfg *cfg,const char *section, const char *key, float default_value)
{
	const char *str=cfg_get_string(cfg,section,key,NULL);
	float ret=default_value;
	if (str==NULL) {
		return default_value;
	}
	sscanf(str,"%f",&ret);
	return ret;
}

void cfg_set_string(Cfg *cfg,const char *section, const char *key, const char *value)
{
	CfgItem *item=NULL;
	CfgSection *sec= NULL;

	if( NULL == cfg )	 return ;
	
	sec = cfg_find_section(cfg,section);
	if (sec!=NULL){
		item=cfg_section_find_item(sec,key);
		if (item!=NULL){
			if (value!=NULL){
				cfg_item_set_value(item,value);
			}else {
				cfg_section_remove_item(sec,item);
			}
		}else{
			if (value!=NULL){
				cfg_section_add_item(sec, cfg_item_new(key,value));
			}
		}
	}else if (value!=NULL)	{
		sec=cfg_section_new(section);
		cfg_add_section(cfg,sec);
		cfg_section_add_item(sec, cfg_item_new(key,value));
	}
	cfg->modified++;
}

void cfg_set_int(Cfg *cfg,const char *section, const char *key, int value)
{
	char tmp[30]={0};
	if( NULL == cfg )  return ;
	snprintf(tmp,sizeof(tmp),"%i",value);
	cfg_set_string(cfg,section,key,tmp);
}

void cfg_set_float(Cfg *cfg,const char *section, const char *key, float value)
{
	char tmp[30] ={0};
	if( NULL == cfg )  return ;
	snprintf(tmp,sizeof(tmp),"%f",value);
	cfg_set_string(cfg,section,key,tmp);
}

void cfg_item_write(CfgItem *item, FILE *file)
{
	fprintf(file,"%s=%s\n",item->key,item->value);
}

void cfg_section_write(CfgSection *sec, FILE *file)
{
	fprintf(file,"[%s]\n",sec->name);
	cfg_list_for_each2(sec->items,(void (*)(void*, void*))cfg_item_write,(void *)file);
	fprintf(file,"\n");
}

int cfg_sync(Cfg *cfg)
{
	FILE *file;
	if (NULL == cfg ||  NULL == cfg->filename) {
		return -1;
	}

#ifndef WIN32
	/* don't create group/world-accessible files */
	(void) umask(S_IRWXG | S_IRWXO);
#endif
	file=fopen(cfg->filename,"w");
	if (file==NULL)	{
		printf("Could not write %s ! Maybe it is read-only. Configuration will not be saved.\n",cfg->filename);
		return -1;
	}
	cfg_list_for_each2(cfg->sections,(void (*)(void *,void*))cfg_section_write,(void *)file);
	fclose(file);
	cfg->modified=0;
	return 0;
}

int cfg_has_section(Cfg *cfg, const char *section)
{
	if (cfg_find_section(cfg,section)!=NULL)
	{
		return 1;
	}
	return 0;
}

void cfg_clean_section(Cfg *cfg, const char *section)
{
	CfgSection *sec=cfg_find_section(cfg,section);
	if (sec!=NULL)
	{
		cfg_remove_section(cfg,sec);
	}
	cfg->modified++;
}

int cfg_needs_commit(const Cfg *cfg)
{
	return cfg->modified>0;
}


