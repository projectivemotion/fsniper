/* keyvalcfg - a key->value config file parser (and writer.)
 * Copyright (C) 2006  Javeed Shaikh <syscrash2k@gmail.com>

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA */


#include "keyvalcfg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PICESIZE 512

#define DEBUG 0

#define IS_SPACE(c) (((c) == ' ') || ((c) == '\t') || ((c) == '\n'))
#define IS_END_KEY(c) (((c) == '\0') || ((c) == '{') || ((c) == '=') || ((c) == '}'))
#define IS_END_VALUE(c) (((c) == '\0') || ((c) == '}') || ((c) == '\n'))

#if 0
/* writes 'section' and all its children to 'file' */
static void keyval_section_write(struct keyval_section * section,
	unsigned recursion, FILE * file) {
	
	struct keyval_pair * pair;
	unsigned count = recursion;
	struct keyval_section * children = section->children;
	char * recursion_tabs = malloc(recursion + 1); /* one tab per recurse */
	recursion_tabs[0] = '\0';

	while (count--) strncat(recursion_tabs, "\t", 1);

	if (section->name) {
		fprintf(file, "%s%s {\n", recursion_tabs + 1, section->name);
	}

	/* write out all of the key value pairs in this section */
	pair = section->keyvals;
	while (pair) {
		fprintf(file, "%s%s = %s;\n", recursion_tabs, pair->key, pair->value);
		pair = pair->next;
	}

	/* recurse (for child sections) */
	while (children) {
		keyval_section_write(children, recursion + 1, file);
		children = children->next;
	}

	if (section->name) fprintf(file, "%s}\n", recursion_tabs + 1);

	free(recursion_tabs);
}

unsigned char keyval_write(struct keyval_section * section,
	const char * filename) {

	FILE * file = stdout;
	if (filename && !(file = fopen(filename, "w"))) return 0;

	keyval_section_write(section, 0, file);
	if (filename) fclose(file);

	return 1;
}
#endif

void keyval_node_write(struct keyval_node * node, size_t depth, FILE * file) {
	char * tabs = malloc(depth + 1);
	size_t count = depth;
	while (count--) strncat(tabs, "\t", 1);
	
	if (node->children) {
		struct keyval_node * child = node->children;
		fprintf(file, "%s%s {\n", tabs, node->name);

		while (child) {
			keyval_node_write(child, depth + 1, file);
			child = child->next;
		}
		fprintf(file, "%s}\n", tabs);
	} else {
		fprintf(file, "%s%s = %s\n", tabs, node->name, node->value);
	}
}

/* returns 'data' + some offset (skips leading whitespace) */
char * skip_leading_whitespace(char * data) {
	while (*data && IS_SPACE(*data)) {
		data++;
	}

	return data;
}

/* returns the length of 'string' if trailing whitespace was to be
 * removed. */
size_t skip_trailing_whitespace(char * string) {
	size_t len = strlen(string) - 1;

	while (IS_SPACE(string[len])) len--;

	return len + 1;
}

/* returns a null-terminated string containing the first n characters of
 * 'source', with trailing whitespace removed. */
char * sanitize_str(char * source, size_t n) {
	char * ret = malloc(n + 1);
	size_t len;

	/* copy the first n chars of source into ret */
	ret = strncpy(ret, source, n);
	ret[n] = '\0';

	/* remove trailing whitespace */
	len = skip_trailing_whitespace(ret);

	if (len != n) {
		ret = realloc(ret, len + 1);
		ret[len] = '\0';
	}

	return ret;
}

/* removes c-style comments from 'string'; replaces them with whitespace.
 * not the most elegant method of doing this (it modifies the argument it
 * receives.) */
/*static void strip_comments(char * string) {
	for (; *string; string++) {
		if (string[0] == '/' && string[1] == '*') {*/
				/* we're starting a comment */
/*				size_t pos = 0;
				while (string[pos] && !(string[pos] == '*' && string[pos + 1] == '/')) {
					string[pos] = ' ';
					pos++;
				}
				if (!string[pos]) return;
				string[pos] = ' ';
				string[pos + 1] = ' ';

				string += pos + 1;
		}
	}
}*/

/* removes # comments from 'string' and replaces them with whitespace. */
void strip_comments(char * string) {
	for (; *string; string++) {
		if (*string == '#') {
			/* we've found a comment. it should last until the end of the line. */
			for (; *string && (*string != '\n'); string++) {
				*string = ' ';
			}
		}
	}
}

/* collapses multi-line statements into one line. the '\' character is used to
 * indicate that a statement continues onto the next line. */
void collapse(char * string) {
	for (; *string; string++) {
		if (*string == '\\') {
			/* the dreaded \ has been encountered. first change this character into
			 * a space. then change the end-of-line character into a space if there
			 * is no 'data' between this \ and the end of the line. */
			char * pos = string++;
			
			for (; *string; string++) {
				if (*string == '\n') {
					*string = ' ';
					/* also change the \ into a space */
					*pos = ' ';
				} else if (*string != '\t' && *string != ' ') {
					/* we have found a non-space character between the \ and the
					 * end-of-line. abort mission. */
					 break;
				}
			}
		}
	}
}

/* transforms sequences of more than one space into a single space. returns a
 * new string which must be freed. */
char * strip_multiple_spaces(char * string) {
	size_t len = 0;
	char * result = malloc(sizeof(char) * (strlen(string) + 1));
	unsigned char seen_space = 0;
	char space_char = 0;

	for (;;) {
		if (IS_SPACE(*string)) {
			seen_space++;
			space_char = *string;
		} else {
			if (seen_space) {
				result[len++] = (seen_space == 1) ? space_char : ' ';
				seen_space = 0;
			}

			result[len++] = *string;
		}

		if (*string == '\0') break;
		string++;
	}

	result[len] = '\0';

	return realloc(result, len + 1);
}

unsigned char is_end_key(char c) {
	if (c == '\0') return 1;
	if (c == '=') return 1;
	if (c == '{') return 1;
	
	return 0;
}

/* the parser. probably full of bugs. ph34r.
 * stops if it encounters } or end of string. */
struct keyval_node * keyval_parse_node(char ** _data, size_t indent) {
	struct keyval_node * head = malloc(sizeof(struct keyval_node));
	struct keyval_node * child = NULL; /* last child found */

	char * data = *_data;

	head->head = head;
	head->name = head->value = NULL;
	head->children = head->next = NULL;



	while (*data) {
		size_t count = 0;
		struct keyval_node * node;
		char * name;

		data = skip_leading_whitespace(data);
		
		/* obscure bug lying in wait. i had it as data[count++] but the macro
		 * expansion pwnt it. don't make the same mistake! */
		while (1) {
			if (IS_END_KEY(data[count])) {
				break;
			}
			count++;
		}

		if (data[count] == '}') {
			data += count + 1;
			break;
		}

		/* count now contains the length of the key + trailing whitespace... with
		 * some offset */
		name = sanitize_str(data, count);
		/* is this node just a key-value pair or is it a section? */
		if (data[count] == '=') {
			/* it's a key-value pair. */
			data = skip_leading_whitespace(data + count + 1);

			/* the value is all characters until some...*/

			if ((*data == '\0') || (*data == '\n')) {
				/* malformed... expected a value, got end of line */
			}

			count = 0;
			/* how many characters occur until end of line? */
			while (1) {
				if (IS_END_VALUE(data[count])) {
					/*printf("%c fails\n", data[count]);*/
					break;
				} else {
					/*printf("%c passes\n", data[count]);*/
				}
				count++;
			}

			/* note that count is now the position of the newline + 1, so it is 1
			 * more than the length of the value. */

			node = malloc(sizeof(struct keyval_node));
			node->value = sanitize_str(data, count);
			node->next = NULL;

			/* done! */
			data += count;
			if (*data && (*data != '}')) data++;

		} else if (data[count] == '{') {
			/* it's a section. */
			char * d = data + count + 1;
			node = keyval_parse_node(&d, indent + 1);
			data = d;
		} else {
			/* stop. */
			break;
		}

		node->name = name;

		if (child) {
			child->next = node;
		} else {
			head->children = node;
		}

		child = node;
	}

	*_data = data;
	return head;
}

struct keyval_node * keyval_parse(const char * filename) {
	char buf[PICESIZE];
	FILE * file = fopen(filename, "r");
	char * data = malloc(PICESIZE);
	char * data2 = data;

	struct keyval_node * node;

	data[0] = '\0';
	if (!file) return NULL;

	while(fgets(buf, PICESIZE, file)) {
		data = realloc(data, strlen(data) + strlen(buf) + 1);
		data = strcat(data, buf);
	}
	fclose(file);

	node = keyval_parse_node(&data2, 0);
	free(data);

	return node;
}
#if 0
/* the convenience functions for getting values and shizer. */
unsigned char keyval_pair_get_value_bool(struct keyval_pair * pair) {
	switch (*pair->value) {
		case 't':
		case 'T':
		case 'y':
		case 'Y':
		case '1':
			return 1;
		default:
			return 0;
	}
}

char * keyval_pair_get_value_string(struct keyval_pair * pair) {
	return strdup(pair->value);
}

int keyval_pair_get_value_int(struct keyval_pair * pair) {
	return atoi(pair->value);
}

double keyval_pair_get_value_double(struct keyval_pair * pair) {
	return strtod(pair->value, NULL);
}
#endif
