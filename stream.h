#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* #include <string.h> */

enum
{
	STREAM_SEEK_BEG,
	/* STREAM_SEEK_SET, */
	STREAM_SEEK_CUR,
	STREAM_SEEK_END
};

/* enum */
/* { */
/* 	STREAM_RESULT_OK, */
/* 	STREAM_RESULT_ERR, */
/* }; */

/* typedef int32_t StreamResult; */
typedef struct Stream_s
{
	/* char filename[256]; */
	/* unsigned char *buffer; */
	/* unsigned int offset, length; */
	void *ctx;

	int64_t (*tell)(struct Stream_s *s);
	/* This function returns zero if successful, or else it returns a non-zero value. */
	int (*seek)(struct Stream_s *s, int64_t offset, int whence);

	int (*name)(struct Stream_s *stream, char *buffer, size_t size);
	int (*eof)(struct Stream_s *stream);
	size_t (*read)(struct Stream_s *stream, void *ptr, size_t size, size_t nmemb);
	/* void (*close)(struct Stream_s *stream); */
	size_t (*write)(struct Stream_s *stream, const void *ptr, size_t size, size_t nmemb);
} Stream;

static size_t stream_read_buffer(Stream *s, void *ptr, size_t n)
{
	return s->read(s, ptr, n, 1);
}
#define stream_read(s, ptr) stream_read_buffer(&(s), &(ptr), sizeof(ptr))

static int stream_measure_line(Stream *s, size_t *n)
{
	*n = 0;

	int eol = 0;
	int eof = 0;
	size_t offset = 0;
	while(!eol)
	{
		uint8_t ch = 0;
		if(0 == s->read(s, &ch, 1, 1) || !ch)
		{
			eof = 1;
			break;
		}
		switch(ch)
		{
			case '\r': eol = 1; break; // In this case, match \r as eol because we don't want carriage returns in our output.
			case '\n': eol = 1; break;
			default: *n += 1; break;
		}
	}
	return eof;
}

static int stream_read_line_cr(Stream *s, char *line, size_t max_line_length, bool *carriage_return)
{
	*carriage_return = false;
	size_t n = 0;
	line[n] = 0;

	int eol = 0;
	int eof = 0;
	size_t offset = 0;
	while(!eol)
	{
		uint8_t ch = 0;
		if(0 == s->read(s, &ch, 1, 1) || !ch)
		{
			// If we haven't read anything yet then this is the "real" EOF
			// Had we encountered a \0 or EOF at the end of a line then it would have been one line too early
			if(n == 0)
				eof = 1;
			break;
		}
		if(n + 1 >= max_line_length) // n + 1 account for \0
		{
			fprintf(stderr, "Error line length %d is larger than the maximum length of a line.\n", max_line_length);
			exit(-1);
		}
		switch(ch)
		{
			case '\r': *carriage_return = true; break;
			case '\n': eol = 1; break;
			default:
				*carriage_return = false;
				line[n++] = ch;
				break;
		}
	}
	line[n] = 0;
	return eof;
}

static int stream_read_line(Stream *s, char *line, size_t max_line_length)
{
	size_t n = 0;
	line[n] = 0;

	int eol = 0;
	int eof = 0;
	size_t offset = 0;
	while(!eol)
	{
		uint8_t ch = 0;
		if(0 == s->read(s, &ch, 1, 1) || !ch)
		{
			// If we haven't read anything yet then this is the "real" EOF
			// Had we encountered a \0 or EOF at the end of a line then it would have been one line too early
			if(n == 0)
				eof = 1;
			break;
		}
		// Instead of stop parsing halfway through the line, just truncate the output instead
		// if(n + 1 >= max_line_length) // n + 1 account for \0
		// {
		// 	break;
		// }
		switch(ch)
		{
			case '\r': break;
			case '\n': eol = 1; break;
			default:
				if(n + 1 < max_line_length) // account for \0
					line[n++] = ch;
			break;
		}
	}
	line[n] = 0;
	return eof;
}

static void stream_unget(Stream *s)
{
	s->seek(s, s->tell(s) - 1, STREAM_SEEK_BEG);
}

static void steam_advance(Stream *s)
{
	s->seek(s, s->tell(s) + 1, STREAM_SEEK_BEG);
}

static uint8_t stream_advance(Stream *s)
{
	uint8_t ch = 0;
	s->read(s, &ch, 1, 1);
	return ch;
}

static uint8_t stream_current(Stream *s)
{
	uint8_t ch = 0;
	if(1 == s->read(s, &ch, 1, 1))
	{
		stream_unget(s);
	}
	return ch;
}

static void stream_print(Stream *s, const char *text)
{
	s->write(s, text, 1, strlen(text) + 1);
	stream_unget(s);
}

static void stream_printf(Stream *s, const char *fmt, ...)
{
	char text[2048] = { 0 };
	if(fmt)
	{
		va_list va;
		va_start(va, fmt);
		vsnprintf(text, sizeof(text), fmt, va);
		va_end(va);
	}
	s->write(s, text, 1, strlen(text) + 1);
	stream_unget(s);
}

static void stream_skip_characters(Stream *s, const char *chars)
{
	while(1)
	{
		uint8_t ch = 0;
		if(0 == s->read(s, &ch, 1, 1) || !ch)
		{
			break;
		}
		bool skip = false;
		for(size_t i = 0; chars[i]; ++i)
		{
			if(chars[i] == ch)
			{
				skip = true;
				break;
			}
		}
		if(!skip)
		{
			stream_unget(s);
			break;
		}
	}
}

/* static size_t stream_read_(struct Stream_s *stream, void *ptr, size_t size, size_t nmemb) */
/* { */
/* 	size_t nb = size * nmemb; */
/* 	if(stream->offset + nb >= stream->length) */
/* 		return 0; // EOF */
/* 	memcpy(ptr, &stream->buffer[stream->offset], nb); */
/* 	stream->offset += nb; */
/* 	return nb; */
/* } */

/* static int stream_from_memory(Stream *stream, void *ptr, size_t size, const char *filename) */
/* { */
/* 	stream->buffer = ptr; */
/* 	stream->length = size; */
/* 	stream->offset = 0; */
/* 	stream->read = stream_read_; */
/* 	if(filename) */
/* 	{ */
/* 		snprintf(stream->filename, sizeof(stream->filename), "%s", filename); */
/* 	} */
/* 	return 0; */
/* } */
#if 0
int stream_open(Stream *stream, const char *filename, const char *mode)
{
	//...
	return 1;
}

void stream_close(Stream *stream)
{
	//...
}
#endif
