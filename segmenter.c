/*
 * Copyright (c) 2009 Chase Douglas
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libavformat/avformat.h"

 struct segment_context {
  unsigned int precision;
  char *base_url;
  char *index_file;
  char *input;
  unsigned int segment_duration;
  char *output_prefix;
  FILE *index_fp;
}; 

void display_usage()
{
  printf("Usage: segmenter [OPTION]...\n");
  printf("\n");
  printf("HTTP Live Streaming - Segments TS file and creates M3U8 index.");
  printf("\n");
  printf("-i\t TS file to segment (Use - for stdin)\n");
  printf("-t\t Duration of each segment (default: 10 seconds)\n");
  printf("-o\t Prefix for the TS segments, will be appended\n");
  printf("-p\t Index Precision (1.ts, 01.ts, 001.ts, default: 1)\n");
  printf("-I\t M3U8 index filename\n");
  printf("-b\t Base url for web address of segments, e.g. http://example.org/video/\n");
  printf("-h\t This help\n");
  printf("\n");
  printf("\n");

  exit(0);
}

void
segment_context_free(struct segment_context * ctx){
}

int 
index_file_write_headers(struct segment_context * ctx){
}

int
index_file_open(struct segment_context * ctx){
  char *write_buf;

  if(ctx->index_file == NULL)
    return -1;

  ctx->index_fp = fopen(ctx->index_file, "w");
  if (!ctx->index_fp) {
    fprintf(stderr, "Could not open temporary m3u8 index file (%s), no index file will be created\n", ctx->index_file);
    exit(1);
  }

  write_buf = malloc(sizeof(char) * 1024);
  if (!write_buf) {
    fprintf(stderr, "Could not allocate write buffer for index file, index file will be invalid\n");
    fclose(ctx->index_fp);
    exit(1);
  }

  snprintf(write_buf, 1024, "#EXTM3U\n#EXT-X-TARGETDURATION:%u\n", ctx->segment_duration);
  if (fwrite(write_buf, strlen(write_buf), 1, ctx->index_fp) != 1) {

    fprintf(stderr, "Could not write to m3u8 index file, will not continue writing to index file\n");
    free(write_buf);
    fclose(ctx->index_fp);
    exit(1);
  }

  free(write_buf);
  return 0;
}


void 
index_file_close(struct segment_context * ctx){
  char *write_buf;

  write_buf = malloc(sizeof(char) * 1024);
  if (!write_buf) {
    fprintf(stderr, "Could not allocate write buffer for index file, index file will be invalid\n");
    fclose(ctx->index_fp);
    exit(1);
  }

  snprintf(write_buf, 1024, "#EXT-X-ENDLIST\n");
  if (fwrite(write_buf, strlen(write_buf), 1, ctx->index_fp) != 1) {
    fprintf(stderr, "Could not write last file and endlist tag to m3u8 index file\n");
    free(write_buf);
    fclose(ctx->index_fp);
    exit(1);
  }

  free(write_buf);
  fclose(ctx->index_fp);
}


void 
index_file_write_segment(struct segment_context * ctx, const unsigned int segment_duration,  char *filename){
  char *write_buf;

  write_buf = malloc(sizeof(char) * 1024);
  if (!write_buf) {
    fprintf(stderr, "Could not allocate write buffer for index file, index file will be invalid\n");
    fclose(ctx->index_fp);
    exit(1);
  }

  sprintf(write_buf, "#EXTINF:%u,\n%s%s\n", segment_duration, ctx->base_url, filename);
  if (fwrite(write_buf, strlen(write_buf), 1, ctx->index_fp) != 1) {
    fprintf(stderr, "Could not write to m3u8 index file, will not continue writing to index file\n");
    free(write_buf);
    fclose(ctx->index_fp);
    exit(1);
  }
}

static AVStream *
add_output_stream(AVFormatContext *output_format_context, AVStream *input_stream) {
  AVCodecContext *input_codec_context;
  AVCodecContext *output_codec_context;
  AVStream *output_stream;

  output_stream = avformat_new_stream(output_format_context, NULL);

  if (!output_stream) {
    fprintf(stderr, "Could not allocate stream\n");
    exit(1);
  }else{
    output_stream->id = 0;
  }

  input_codec_context = input_stream->codec;
  output_codec_context = output_stream->codec;

  output_codec_context->codec_id = input_codec_context->codec_id;
  output_codec_context->codec_type = input_codec_context->codec_type;
  output_codec_context->codec_tag = input_codec_context->codec_tag;
  output_codec_context->bit_rate = input_codec_context->bit_rate;
  output_codec_context->extradata = input_codec_context->extradata;
  output_codec_context->extradata_size = input_codec_context->extradata_size;

  if(av_q2d(input_codec_context->time_base) * input_codec_context->ticks_per_frame > av_q2d(input_stream->time_base) && av_q2d(input_stream->time_base) < 1.0/1000) {
    output_codec_context->time_base = input_codec_context->time_base;
    output_codec_context->time_base.num *= input_codec_context->ticks_per_frame;
  }
  else {
    output_codec_context->time_base = input_stream->time_base;
  }

  switch (input_codec_context->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
    output_codec_context->channel_layout = input_codec_context->channel_layout;
    output_codec_context->sample_rate = input_codec_context->sample_rate;
    output_codec_context->channels = input_codec_context->channels;
    output_codec_context->frame_size = input_codec_context->frame_size;
    if ((input_codec_context->block_align == 1 && input_codec_context->codec_id == CODEC_ID_MP3) || input_codec_context->codec_id == CODEC_ID_AC3) {
      output_codec_context->block_align = 0;
    }
    else {
      output_codec_context->block_align = input_codec_context->block_align;
    }
    break;
    case AVMEDIA_TYPE_VIDEO:
    output_codec_context->pix_fmt = input_codec_context->pix_fmt;
    output_codec_context->width = input_codec_context->width;
    output_codec_context->height = input_codec_context->height;
    output_codec_context->has_b_frames = input_codec_context->has_b_frames;

    if (output_format_context->oformat->flags & AVFMT_GLOBALHEADER) {
      output_codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    break;
    default:
    break;
  }

  return output_stream;
}

void
create_segments(struct segment_context * ctx) {
 long max_tsfiles = 0;
 char *max_tsfiles_check;
 double prev_segment_time = 0;
 unsigned int output_index = 1;

 AVInputFormat *ifmt;
 AVOutputFormat *ofmt;
 AVFormatContext *ic = NULL;
 AVFormatContext *oc;
 AVStream *video_st = NULL;
 AVStream *audio_st = NULL;;
 AVCodec *codec;

 char *output_filename;
 char *output_format;

 int video_index;
 int audio_index;

 unsigned int first_segment = 1;
 unsigned int last_segment = 0;

 int decode_done;
 int ret;
 int i;
 int remove_file;
 int write_index = -1;

 av_register_all();

 ifmt = av_find_input_format("mpegts");
 if (!ifmt) {
   fprintf(stderr, "Could not find MPEG-TS demuxer\n");
   exit(1);
 }

 ret = avformat_open_input(&ic, ctx->input, ifmt, NULL);
 if (ret != 0) {
   fprintf(stderr, "Could not open input file, make sure it is an mpegts file: %d\n", ret);
   exit(1);
 }

 if (avformat_find_stream_info(ic, NULL) < 0) {
  fprintf(stderr, "Could not read stream information\n");
  exit(1);
}

ofmt = av_guess_format("mpegts", NULL, NULL);
if (!ofmt) {
  fprintf(stderr, "Could not find MPEG-TS muxer\n");
  exit(1);
}

write_index = index_file_open(ctx);

if(write_index >= 0)
  index_file_write_headers(ctx);

oc = avformat_alloc_context();
if (!oc) {
  fprintf(stderr, "Could not allocated output context");
  exit(1);
}
oc->oformat = ofmt;

video_index = -1;
audio_index = -1;

for (i = 0; i < ic->nb_streams && (video_index < 0 || audio_index < 0); i++) {
 switch (ic->streams[i]->codec->codec_type) {
   case AVMEDIA_TYPE_VIDEO:
   video_index = i;
   ic->streams[i]->discard = AVDISCARD_NONE;
   video_st = add_output_stream(oc, ic->streams[i]);
   break;
   case AVMEDIA_TYPE_AUDIO:
   audio_index = i;
   ic->streams[i]->discard = AVDISCARD_NONE;
   audio_st = add_output_stream(oc, ic->streams[i]);
   break;
   default:
   ic->streams[i]->discard = AVDISCARD_ALL;
   break;
 }
}

av_dump_format(oc, 0, ctx->output_prefix, 1);

if(video_st) {
  codec = avcodec_find_decoder(video_st->codec->codec_id);
  if (!codec) {
    fprintf(stderr, "Could not find video decoder, key frames will not be honored\n");
  }

  if (avcodec_open2(video_st->codec, codec, NULL) < 0) {
   fprintf(stderr, "Could not open video decoder, key frames will not be honored\n");
 }
}

output_filename = malloc(sizeof(char) * (strlen(ctx->output_prefix) + 15));
if (!output_filename) {
 fprintf(stderr, "Could not allocate space for output filename\n");
 exit(1);
}

output_format = malloc(sizeof(char) * (strlen(ctx->output_prefix) + 15));
if (!output_format) {
 fprintf(stderr, "Could not allocate space for output format\n");
 exit(1);
}
sprintf(output_format, "%%s-%%0%uu.ts", ctx->precision);

sprintf(output_filename, output_format, ctx->output_prefix, output_index++);

if (avio_open(&oc->pb, output_filename, AVIO_FLAG_WRITE) < 0) {
  fprintf(stderr, "Could not open '%s'\n", output_filename);
  exit(1);
}

if (avformat_write_header(oc, NULL) < 0) {
 fprintf(stderr, "Could not write mpegts header to first output file\n");
 exit(1);
}

AVPacket packet;

do {

  double segment_time;

  decode_done = av_read_frame(ic, &packet);
  if (decode_done < 0) {
    break;
  }

  if (av_dup_packet(&packet) < 0) {
    fprintf(stderr, "Could not duplicate packet");
    av_free_packet(&packet);
    break;
  }

  if (packet.stream_index == video_index && (packet.flags & AV_PKT_FLAG_KEY)) {
    segment_time = packet.pts * av_q2d(video_st->time_base);
  }
  else if (video_index < 0) {
    segment_time = packet.pts * av_q2d(audio_st->time_base);
  }
  else {
    segment_time = prev_segment_time;
  }

  if (segment_time - prev_segment_time >= ctx->segment_duration) {
    avio_flush(oc->pb);
    avio_close(oc->pb);

    if(write_index >= 0)
      index_file_write_segment(ctx, floor(segment_time - prev_segment_time), output_filename);

    sprintf(output_filename, output_format, ctx->output_prefix, output_index++);

    if (avio_open(&oc->pb, output_filename, AVIO_FLAG_WRITE) < 0) {
      fprintf(stderr, "Could not open '%s'\n", output_filename);
      break;
    }

    prev_segment_time = segment_time;
  }

  ret = av_interleaved_write_frame(oc, &packet);

  if (ret < 0) {
    fprintf(stderr, "Warning: Could not write frame of stream\n");
    break;
  }
  else if (ret > 0) {
    fprintf(stderr, "End of stream requested\n");
    av_free_packet(&packet);
    break;
  }

  av_free_packet(&packet);
} while (1); /* loop is exited on break */

double input_duration = (double)ic->duration / 1000000;

if(write_index >= 0)
 index_file_write_segment(ctx, ceil(input_duration - prev_segment_time), output_filename);

av_write_trailer(oc);

if(video_st)
avcodec_close(video_st->codec);

for(i = 0; i < oc->nb_streams; i++) {
  av_freep(&oc->streams[i]->codec);
  av_freep(&oc->streams[i]);
}

avio_close(oc->pb);
av_free(oc);

if(write_index >= 0)
  index_file_close(ctx);
}

void 
segment_context_default(struct segment_context *ctx) {
  ctx->base_url = "";
  ctx->index_file = NULL;
  ctx->input = "pipe:";
  ctx->precision = 1;
  ctx->segment_duration = 10;
  ctx->output_prefix = NULL;
}

int
main (int argc, char **argv)
{
  struct segment_context ctx;

  segment_context_default(&ctx);

  char *segment_duration_check;
  int index;
  int c;

  while ((c = getopt (argc, argv, "b:t:I:o:p:i:h")) != -1)
    switch (c)
    {
      case 'b':
      ctx.base_url = optarg;
      break;
      case 'p':
      ctx.precision = atoi(optarg);
      break;
      case 'o':
      ctx.output_prefix = optarg;
      break;
      case 'i':
      ctx.input = optarg;
      break;
      case 'I':
      ctx.index_file = optarg;
      break;
      case 'h':
      display_usage();
      break;
      case 't':
      ctx.segment_duration = strtod(optarg, &segment_duration_check);
      if (segment_duration_check == optarg || ctx.segment_duration == HUGE_VAL || ctx.segment_duration == -HUGE_VAL) {
        fprintf(stderr, "Segment duration time (%s) invalid\n", optarg);
        exit(1);
      }
      break;

      default:
      exit(1);
    }

  if(argc < 1) {
    display_usage();
  }

  if (ctx.input == NULL && ctx.index_file) {
    fprintf(stderr, "Using stdin as input.\n");
  }

  if(ctx.output_prefix == NULL && ctx.index_file != NULL) {

    char * dot = strrchr(ctx.index_file,'.');
    if (dot == NULL) {
      fprintf(stderr,"Problem with name in file: %s\n", ctx.index_file);
      exit(4);
    }

    size_t size = strlen(ctx.index_file) - strlen(dot);
    ctx.output_prefix = (char *)malloc(size);
    strncpy(ctx.output_prefix, ctx.index_file , size);
  }

  if (ctx.output_prefix == NULL) {
    fprintf(stderr,"Output prefix unspecified\n");
    exit(4);
  }

  create_segments(&ctx);

  segment_context_free(&ctx);
  return 0;
}

