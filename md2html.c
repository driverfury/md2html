#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

typedef struct string_view string_view;

struct string_view
{
    char *str;
    size_t len;
};

string_view sv_trim_start(string_view sv)
{
    string_view result = sv;

    while (result.len > 0 && isspace(*result.str))
    {
	result.str++;
	result.len--;
    }

    return result;
}

string_view sv_trim_end(string_view sv)
{
    string_view result = sv;

    while (result.len > 0 && isspace(result.str[result.len - 1]))
    {
	result.len--;
    }

    return result;
}

string_view sv_trim(string_view sv)
{
    return sv_trim_start(sv_trim_end(sv));
}

string_view sv_substr(string_view sv, size_t start_index, size_t len)
{
    string_view result = sv;

    if (start_index >= sv.len)
    {
	result.len = 0;
	result.str = 0;
	return result;
    }

    size_t max_len = sv.len - start_index;
    len = (len > max_len) ? max_len : len;

    result.str = sv.str + start_index;
    result.len = len;

    return result;
}

static string_view get_line(char **source_ptr)
{
    string_view line;
    line.str = *source_ptr;
    line.len = 0;

    while (**source_ptr)
    {
	if (**source_ptr == '\n')
	{
	    (*source_ptr)++;
	    break;
	}
	else if (**source_ptr == '\r')
	{
	    (*source_ptr)++;
	    if (**source_ptr == '\n')
	    {
		(*source_ptr)++;
	    }

	    break;
	}
	else
	{
	    line.len++;
	    (*source_ptr)++;
	}
    }

    return line;
}

static bool heading_comes_next(char *source_ptr)
{
    if (!source_ptr[0] == '#')
    {
	return false;
    }

    unsigned int level = 0;
    while (*source_ptr == '#')
    {
	source_ptr++;
	level++;
    }

    if (level == 0 || level > 6)
    {
	return false;
    }

    if (*source_ptr != ' ')
    {
	return false;
    }

    return true;
}

static bool block_quote_comes_next(char *source_ptr)
{
    if (source_ptr[0] == '>' && source_ptr[1] == ' ')
    {
	return true;
    }

    return false;
}

static bool thematic_break_comes_next(char *source_ptr)
{
    if (source_ptr[0] == '-' && source_ptr[1] == '-' && source_ptr[2] == '-')
    {
	return true;
    }

    return false;
}

static bool unordered_list_item_comes_next(char *source_ptr)
{
    if (source_ptr[0] == '-' && source_ptr[1] == ' ')
    {
	return true;
    }

    return false;
}

static bool ordered_list_item_comes_next(char *source_ptr)
{
    char *ptr = source_ptr;
    if (*ptr == 0 || !isdigit(*ptr))
    {
	return false;
    }

    while (isdigit(*ptr))
    {
	ptr++;
    }

    if (ptr[0] != '.' || ptr[1] != ' ')
    {
	return false;
    }

    return true;
}

static bool fenced_code_comes_next(char *source_ptr)
{
    if (source_ptr[0] == '`' && source_ptr[1] == '`' && source_ptr[2] == '`')
    {
	return true;
    }

    return false;
}

static bool quick_link_comes_next(char *source_ptr)
{
    if (source_ptr[0] != '<')
    {
	return false;
    }

    source_ptr += 1;

    while (isgraph(*source_ptr) && *source_ptr != '>')
    {
	source_ptr++;
    }

    if (*source_ptr != '>')
    {
	return false;
    }

    return true;
}

static bool link_comes_next(char *source_ptr)
{
    if (source_ptr[0] != '[')
    {
	return false;
    }

    source_ptr += 1;

    size_t text_len = 0;
    while (source_ptr[text_len] && source_ptr[text_len] != '\n' && source_ptr[text_len] != '\n' && source_ptr[text_len] != ']')
    {
	text_len++;
    }

    if (text_len == 0 || source_ptr[text_len] != ']')
    {
	return false;
    }

    source_ptr += text_len + 1;

    if (source_ptr[0] != '(' || !isgraph(source_ptr[1]))
    {
	return false;
    }

    source_ptr += 1;
    while (*source_ptr && isgraph(*source_ptr) && *source_ptr != ')')
    {
	source_ptr++;
    }

    if (*source_ptr != ')')
    {
	return false;
    }

    return true;
}

static bool image_comes_next(char *source_ptr)
{
    if (source_ptr[0] != '!')
    {
	return false;
    }

    source_ptr += 1;

    return link_comes_next(source_ptr);
}

static bool footnote_comes_next(char *source_ptr)
{
    if (source_ptr[0] != '[' || source_ptr[1] != '^' || !isdigit(source_ptr[2]))
    {
	return false;
    }

    source_ptr += 3;
    while (isdigit(*source_ptr))
    {
	source_ptr++;
    }

    if (source_ptr[0] != ']' || source_ptr[1] != ':')
    {
	return false;
    }

    return true;
}

static bool footnote_reference_comes_next(char *source_ptr)
{
    if (source_ptr[0] != '[' || source_ptr[1] != '^' || !isdigit(source_ptr[2]))
    {
	return false;
    }

    source_ptr += 3;
    while (isdigit(*source_ptr))
    {
	source_ptr++;
    }

    if (source_ptr[0] != ']')
    {
	return false;
    }

    return true;
}

static void skip_ordered_list_item_prefix(char **source_ptr)
{
    while (isdigit(**source_ptr))
    {
	*source_ptr += 1;
    }

    *source_ptr += 2;
}

static bool char_can_be_escaped(char c)
{
    switch (c)
    {
	case '\\':
	case '`':
	case '*':
	case '_':
	case '{': case '}':
	case '[': case ']':
	case '(': case ')':
	case '<': case '>':
	case '#':
	case '+':
	case '-':
	case '.':
	case '!':
	case '|':
	    return true;

	default:
	    return false;
    }
}

static void compile_paragraph_line(string_view line, FILE *fout, bool *bold_text, bool *italic_text, bool *striked_text, bool *inline_code)
{
    size_t index = 0;
    while (index < line.len)
    {
	if (quick_link_comes_next(line.str + index))
	{
	    index += 1;

	    size_t url_len = 0;
	    while (line.str[index + url_len] != '>')
	    {
		url_len++;
	    }

	    string_view link_url = sv_substr(line, index, url_len);
	    fprintf(fout, "<a href=\"%.*s\">%.*s</a>", (int)link_url.len, link_url.str, (int)link_url.len, link_url.str);

	    index += url_len;
	}
	else if (link_comes_next(line.str + index))
	{
	    index += 1;

	    size_t text_len = 0;
	    while (!(line.str[index + text_len] == ']' && line.str[index + text_len - 1] != '\\'))
	    {
		text_len++;
	    }

	    string_view link_text = sv_substr(line, index, text_len);

	    index += text_len + 1;

	    index += 1;

	    size_t url_len = 0;
	    while (line.str[index + url_len] != ')')
	    {
		url_len++;
	    }

	    string_view link_url = sv_substr(line, index, url_len);

	    index += url_len + 1;

	    fprintf(fout, "<a href=\"%.*s\">%.*s</a>", (int)link_url.len, link_url.str, (int)link_text.len, link_text.str);
	}
	else if (image_comes_next(line.str + index))
	{
	    index += 2;

	    size_t text_len = 0;
	    while (!(line.str[index + text_len] == ']' && line.str[index + text_len - 1] != '\\'))
	    {
		text_len++;
	    }

	    string_view link_text = sv_substr(line, index, text_len);

	    index += text_len + 1;

	    index += 1;

	    size_t url_len = 0;
	    while (line.str[index + url_len] != ')')
	    {
		url_len++;
	    }

	    string_view link_url = sv_substr(line, index, url_len);

	    index += url_len + 1;

	    fprintf(fout, "<img alt=\"%.*s\" src=\"%.*s\" />", (int)link_text.len, link_text.str, (int)link_url.len, link_url.str);
	}
	else if (footnote_reference_comes_next(line.str + index))
	{
	    index += 2;

	    size_t footnote_name_len = 0;
	    while (index + footnote_name_len < line.len && isdigit(line.str[index + footnote_name_len]))
	    {
		footnote_name_len++;
	    }

	    string_view footnote_name;
	    if (footnote_name_len == 0)
	    {
		footnote_name.str = "0";
		footnote_name.len = 1;
	    }
	    else
	    {
		footnote_name = sv_substr(line, index, footnote_name_len);
	    }

	    index += footnote_name_len;

	    fprintf(fout, "<sup id=\"fnref:%.*s\" role=\"doc-noteref\">", (int)footnote_name.len, footnote_name.str);
	    fprintf(fout, "<a href=\"#fn:%.*s\" class=\"footnote\" rel=\"footnote\">", (int)footnote_name.len, footnote_name.str);
	    fprintf(fout, "%.*s", (int)footnote_name.len, footnote_name.str);
	    fprintf(fout, "</a>");
	    fprintf(fout, "</sup>");

	    if (index < line.len && line.str[index] == ']')
	    {
		index += 1;
	    }
	}
	else if (line.str[index] == '\\' && index + 1 < line.len && char_can_be_escaped(line.str[index + 1]))
	{
	    fprintf(fout, "%c", line.str[index + 1]);
	    index += 2;
	}
	else if (line.str[index] == '*' && index + 1 < line.len && line.str[index + 1] == '*')
	{
	    if (*bold_text)
	    {
		fprintf(fout, "</strong>");
	    }
	    else
	    {
		fprintf(fout, "<strong>");
	    }

	    *bold_text = !*bold_text;
	    index += 2;
	}
	else if (line.str[index] == '*')
	{
	    if (*italic_text)
	    {
		fprintf(fout, "</em>");
	    }
	    else
	    {
		fprintf(fout, "<em>");
	    }

	    *italic_text = !*italic_text;
	    index += 1;
	}
	else if (line.str[index] == '~' && index + 1 < line.len && line.str[index + 1] == '~')
	{
	    if (*striked_text)
	    {
		fprintf(fout, "</del>");
	    }
	    else
	    {
		fprintf(fout, "<del>");
	    }

	    *striked_text = !*striked_text;
	    index += 2;
	}
	else if (line.str[index] == '`')
	{
	    if (*inline_code)
	    {
		fprintf(fout, "</code>");
	    }
	    else
	    {
		fprintf(fout, "<code>");
	    }

	    *inline_code = !*inline_code;
	    index += 1;
	}
	else if (line.str[index] == '<')
	{
	    fprintf(fout, "&lt;");
	    index += 1;
	}
	else if (line.str[index] == '>')
	{
	    fprintf(fout, "&gt;");
	    index += 1;
	}
	else
	{
	    fprintf(fout, "%c", line.str[index]);
	    index += 1;
	}
    }

    fprintf(fout, "\n");
}

static void compile_paragraph(char **source_ptr, FILE *fout, bool open_paragraph_tag)
{
    if (open_paragraph_tag)
    {
	fprintf(fout, "<p>\n");
    }

    bool bold_text = false;
    bool italic_text = false;
    bool striked_text = false;
    bool inline_code = false;
    while (true)
    {
	if (**source_ptr == 0 || **source_ptr == '\n' || **source_ptr == '\r' ||
	    heading_comes_next(*source_ptr) ||
	    block_quote_comes_next(*source_ptr) ||
	    thematic_break_comes_next(*source_ptr) ||
	    unordered_list_item_comes_next(*source_ptr) ||
	    ordered_list_item_comes_next(*source_ptr) ||
	    fenced_code_comes_next(*source_ptr))
	{
	    if (open_paragraph_tag)
	    {
		fprintf(fout, "</p>\n");
	    }

	    break;
	}

	string_view line = sv_trim(get_line(source_ptr));
	compile_paragraph_line(line, fout, &bold_text, &italic_text, &striked_text, &inline_code);
    }

    if (bold_text || italic_text || striked_text || inline_code)
    {
	// TODO: log syntax error?
    }
}

static void compile_heading(char **source_ptr, FILE *fout)
{
    unsigned int level = 0;
    while (**source_ptr == '#')
    {
	level++;
	*source_ptr += 1;
    }

    string_view content = sv_trim(get_line(source_ptr));

    fprintf(fout, "<h%u>", level);
    fprintf(fout, "%.*s", (int)content.len, content.str);
    fprintf(fout, "</h%u>\n", level);
}

static void compile_block_quote(char **source_ptr, FILE *fout)
{
    *source_ptr += 1;

    fprintf(fout, "<blockquote>\n");
    compile_paragraph(source_ptr, fout, true);
    fprintf(fout, "</blockquote>\n");
}

static void compile_thematic_break(char **source_ptr, FILE *fout)
{
    get_line(source_ptr);
    fprintf(fout, "<hr />\n");
}

static void compile_unordered_list(char **source_ptr, FILE *fout)
{
    fprintf(fout, "<ul>\n");

    while ((*source_ptr)[0] == '-' && (*source_ptr)[1] == ' ')
    {
	*source_ptr += 2;

	fprintf(fout, "<li>\n");
	compile_paragraph(source_ptr, fout, false);
	fprintf(fout, "</li>\n");
    }

    fprintf(fout, "</ul>\n");
}

static void compile_ordered_list(char **source_ptr, FILE *fout)
{
    fprintf(fout, "<ol>\n");

    while (ordered_list_item_comes_next(*source_ptr))
    {
	if (**source_ptr == 0 || !isdigit(**source_ptr))
	{
	    break;
	}

	skip_ordered_list_item_prefix(source_ptr);

	fprintf(fout, "<li>\n");
	compile_paragraph(source_ptr, fout, false);
	fprintf(fout, "</li>\n");
    }

    fprintf(fout, "</ol>\n");
}

static void compile_fenced_code_line(string_view line, FILE *fout)
{
    size_t index = 0;
    while (index < line.len)
    {
	if (line.str[index] == '<')
	{
	    fprintf(fout, "&lt;");
	    index += 1;
	}
	else if (line.str[index] == '>')
	{
	    fprintf(fout, "&gt;");
	    index += 1;
	}
	else
	{
	    fprintf(fout, "%c", line.str[index]);
	    index += 1;
	}
    }

    fprintf(fout, "\n");
}

static void compile_fenced_code(char **source_ptr, FILE *fout)
{
    string_view line = sv_trim(get_line(source_ptr));
    string_view language = sv_trim(sv_substr(line, 3, line.len - 3));

    fprintf(fout, "<pre><code");
    if (language.len > 0)
    {
	fprintf(fout, " class=\"lang-%.*s\"", (int)language.len, language.str);
    }
    fprintf(fout, ">\n");

    while (**source_ptr && !fenced_code_comes_next(*source_ptr))
    {
	line = get_line(source_ptr);
	compile_fenced_code_line(line, fout);
    }

    get_line(source_ptr);

    fprintf(fout, "</code></pre>\n");
}

static void compile_footnote(char **source_ptr, FILE *fout)
{
    *source_ptr += 2;

    size_t footnote_name_len = 0;
    while ((*source_ptr)[footnote_name_len] && (*source_ptr)[footnote_name_len] != ']')
    {
	footnote_name_len++;
    }

    string_view footnote_name;
    if (footnote_name_len == 0)
    {
	footnote_name.str = "0";
	footnote_name.len = 1;
    }
    else
    {
	footnote_name.str = *source_ptr;
	footnote_name.len = footnote_name_len;
    }

    *source_ptr += footnote_name_len;
    *source_ptr += 2;

    fprintf(fout, "<div id=\"fn:%.*s\" role=\"doc-endnote\">%.*s. ", (int)footnote_name.len, footnote_name.str, (int)footnote_name.len, footnote_name.str);
    string_view line = get_line(source_ptr);

    bool bold_text = false, italic_text = false, striked_text = false, inline_code = false;
    compile_paragraph_line(line, fout, &bold_text, &italic_text, &striked_text, &inline_code);

    fprintf(fout, "</div>");
}

void compile_markdown_to_html(const char *source, FILE *fout)
{
    char *source_ptr = (char *)source;
    while (*source_ptr)
    {
	while (isspace(*source_ptr))
	{
	    source_ptr++;
	}

	if (!*source_ptr)
	{
	    break;
	}
	else if (heading_comes_next(source_ptr))
	{
	    compile_heading(&source_ptr, fout);
	}
	else if (block_quote_comes_next(source_ptr))
	{
	    compile_block_quote(&source_ptr, fout);
	}
	else if (thematic_break_comes_next(source_ptr))
	{
	    compile_thematic_break(&source_ptr, fout);
	}
	else if (unordered_list_item_comes_next(source_ptr))
	{
	    compile_unordered_list(&source_ptr, fout);
	}
	else if (ordered_list_item_comes_next(source_ptr))
	{
	    compile_ordered_list(&source_ptr, fout);
	}
	else if (fenced_code_comes_next(source_ptr))
	{
	    compile_fenced_code(&source_ptr, fout);
	}
	else if (footnote_comes_next(source_ptr))
	{
	    compile_footnote(&source_ptr, fout);
	}
	else
	{
	    compile_paragraph(&source_ptr, fout, true);
	}
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(FILE *f, char *argv0)
{
    fprintf(f, "Usage: %s [-i <input.md>] [-o <output.html>]\n", argv0);
}

int main(int argc, char *argv[])
{
    char *input_file_name = NULL;
    char *output_file_name = NULL;
    if (argc >= 3 && strcmp(argv[1], "-i") == 0)
    {
	input_file_name = argv[2];
    }
    else if(argc >= 3 && strcmp(argv[1], "-o") == 0)
    {
	output_file_name = argv[2];
    }

    if (argc >= 5 && strcmp(argv[3], "-i") == 0)
    {
	input_file_name = argv[4];
    }
    else if(argc >= 5 && strcmp(argv[3], "-o") == 0)
    {
	output_file_name = argv[4];
    }

    size_t source_cap = 512;
    size_t source_len = 0;
    char *source = (char *)malloc(source_cap * sizeof(char));
    {
	FILE *fin = stdin;
	if (input_file_name)
	{
	    fin = fopen(input_file_name, "rb");
	    if (!fin)
	    {
		fprintf(stderr, "Error: cannot open file '%s' for reading", input_file_name);
		return -1;
	    }
	}

	int c;
	while ((c = fgetc(fin)) != EOF)
	{
	    if (source_len >= source_cap)
	    {
		source_cap *= 2;
		source = (char *)realloc(source, source_cap * sizeof(char));
	    }

	    source[source_len++] = (char)c;
	}

	if (source[source_len - 1] != 0)
	{
	    if (source_len >= source_cap)
	    {
		source_cap += 1;
		source = (char *)realloc(source, source_cap * sizeof(char));
	    }

	    source[source_len++] = 0;
	}

	fclose(fin);
    }

    FILE *fout = stdout;
    {
	if (output_file_name)
	{
	    fout = fopen(output_file_name, "w");
	    if (!fout)
	    {
		fprintf(stderr, "Error: cannot open file '%s' for writing", output_file_name);
		return -1;
	    }
	}
    }

    compile_markdown_to_html(source, fout);

    fclose(fout);
    free(source);

    return 0;
}
