#pragma once

#include "stream.h"
#include <string.h>
#include <malloc.h>

typedef struct StreamBuffer_s
{
	size_t offset, length;//, capacity;
	unsigned char *buffer;
	bool (*grow)(struct StreamBuffer_s*, size_t size);
} StreamBuffer;

static size_t stream_read_(struct Stream_s *stream, void *ptr, size_t size, size_t nmemb)
{
	StreamBuffer *sd = (StreamBuffer *)stream->ctx;
	size_t nb = size * nmemb;
	if(sd->offset + nb > sd->length)
	{
		/* printf("overflow offset:%d,nb:%d,length:%d,size:%d,nmemb:%d\n",sd->offset,nb,sd->length,size,nmemb); */
		// return 0; // EOF
		nb = sd->length - sd->offset;
	}
	if(nb == 0)
		return 0;
	memcpy(ptr, &sd->buffer[sd->offset], nb);
	/* printf("reading %d (%d/%d)\n", nb, sd->offset, sd->length); */
	sd->offset += nb;
	return nmemb;
}

static size_t stream_write_(struct Stream_s *stream, const void *ptr, size_t size, size_t nmemb)
{
	StreamBuffer *sd = (StreamBuffer *)stream->ctx;
	size_t nb = size * nmemb;
	if(sd->offset + nb > sd->length)
	{
		if(!sd->grow)
		{
			/* printf("overflow offset:%d,nb:%d,length:%d,size:%d,nmemb:%d\n",sd->offset,nb,sd->length,size,nmemb); */
			return 0; // EOF
		}
		sd->grow(sd, sd->offset + nb);
	}
	memcpy(&sd->buffer[sd->offset], ptr, nb);
	/* printf("writing %d (%d/%d)\n", nb, sd->offset, sd->length); */
	sd->offset += nb;
	return nmemb;
}

static int stream_eof_(struct Stream_s *stream)
{
	StreamBuffer *sd = (StreamBuffer *)stream->ctx;
	return sd->offset >= sd->length;
}

static int stream_name_(struct Stream_s *s, char *buffer, size_t size)
{
	StreamBuffer *sd = (StreamBuffer *)s->ctx;
	buffer[0] = 0;
	return 0;
}

static int64_t stream_tell_(struct Stream_s *s)
{
	StreamBuffer *sd = (StreamBuffer *)s->ctx;
	return sd->offset;
}
#ifndef MIN
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#endif
#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif

static int stream_seek_(struct Stream_s *s, int64_t offset, int whence)
{
	StreamBuffer *sd = (StreamBuffer *)s->ctx;
	int64_t current = 0;
	switch(whence)
	{
		case STREAM_SEEK_BEG:
		{
			current = 0;
		}
		break;
		case STREAM_SEEK_CUR:
		{
			current = (int64_t)sd->offset;
		}
		break;
		case STREAM_SEEK_END:
		{
			current = (int64_t)sd->length;
		}
		break;
	}
	current += offset;
	if(current < 0)
		current = 0;
	if((size_t)current >= sd->length)
	{
		current = sd->length;
	}
	sd->offset = current;
	return 0;
}

static int init_stream_from_stream_buffer(Stream *s, StreamBuffer *sb)
{	
	s->ctx = sb;
	s->read = stream_read_;
	s->write = stream_write_;
	s->eof = stream_eof_;
	s->name = stream_name_;
	s->tell = stream_tell_;
	s->seek = stream_seek_;
	return 0;
}

static int init_stream_from_buffer(Stream *s, StreamBuffer *sb, unsigned char *buffer, size_t length)
{
	sb->offset = 0;
	sb->length = length;
	sb->buffer = buffer;
	// sb->capacity = length;
	sb->grow = NULL;
	
	s->ctx = sb;
	s->read = stream_read_;
	s->write = stream_write_;
	s->eof = stream_eof_;
	s->name = stream_name_;
	s->tell = stream_tell_;
	s->seek = stream_seek_;
	return 0;
}

static bool stream_buffer_buffer_grow_realloc(struct StreamBuffer_s *sb, size_t size)
{
    sb->buffer = (unsigned char*)realloc(sb->buffer, size * 2);
    sb->length = size * 2;
	return true;
}
