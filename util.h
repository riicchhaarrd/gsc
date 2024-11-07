#pragma once
#include <stdio.h>

static void process_escape_sequences(const char *s, FILE *fp)
{
	while(*s)
	{
		if(*s == '\\')
		{
			switch(*++s)
			{
				case 'n': fputc('\n', fp); break;
				case 't': fputc('\t', fp); break;
				case '\\': fputc('\\', fp); break;
				case '"': fputc('\"', fp); break;
				case '\'': fputc('\'', fp); break;
				case 'r': fputc('\r', fp); break;
				case 'b': fputc('\b', fp); break;
				default:
					fputc('\\', fp);
					fputc(*s, fp);
					break;
			}
		}
		else
		{
			fputc(*s, fp);
		}
		s++;
	}
}