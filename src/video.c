/*
 * This file is part of the fdupves package
 * Copyright (C) <2008> Alf
 *
 * Contact: Alf <naihe2010@126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
/* @CFILE video.c
 *
 *  Author: Alf <naihe2010@126.com>
 */

#include "video.h"
#include "util.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/display.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include <glib.h>
#include <math.h>
#include <string.h>

#define FDUPVES_VIDEO_BLACK 24

struct video_s
{
  const char *file;
  AVFormatContext *format_ctx;
  AVStream *stream;
  AVCodecContext *codec_ctx;
  AVFrame *frame;
  AVFrame *next;
  AVPacket *packet;
  gboolean broken;
};

video_t *
video_open (const char *file, double *length)
{
  video_t *video;
  AVFormatContext *format_ctx = NULL;
  AVStream *stream;
  int s;

  if (avformat_open_input (&format_ctx, file, NULL, NULL) != 0)
    {
      g_warning (_ ("could not open: %s"), file);
      return NULL;
    }

  /*
  if (avformat_find_stream_info (format_ctx, NULL) < 0)
    {
      g_warning (_ ("could not find stream infomations: %s"), file);
      avformat_close_input (&format_ctx);
      return NULL;
    }*/

  s = av_find_best_stream (format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (s < 0)
    {
      g_warning (_ ("could not find video stream: %s"), file);
      avformat_close_input (&format_ctx);
      return NULL;
    }
  stream = format_ctx->streams[s];

  if (stream->duration != AV_NOPTS_VALUE)
    {
      *length = (double)(stream->duration * stream->time_base.num)
                / stream->time_base.den;
    }
  else
    {
      *length = (double)(format_ctx->duration) / AV_TIME_BASE;
    }

  video = g_new0 (video_t, 1);
  video->file = file;
  video->format_ctx = format_ctx;
  video->stream = stream;

  return video;
}

video_info *
video_get_info (const char *file)
{
  video_info *info;
  video_t *video;
  double length;

  video = video_open (file, &length);
  if (video == NULL)
    {
      return NULL;
    }

  info = g_malloc0 (sizeof (video_info));

  info->name = g_path_get_basename (file);
  info->dir = g_path_get_dirname (file);
  info->length = length;
  info->size[0] = video->stream->codecpar->width;
  info->size[1] = video->stream->codecpar->height;
  info->format = avcodec_get_name (video->stream->codecpar->codec_id);

  video_close (video);

  return info;
}

void
video_info_free (video_info *info)
{
  g_free (info->name);
  g_free (info->dir);
  g_free (info);
}

static int
video_stream_rotation (const AVStream *stream)
{
  const int32_t *matrix;
  double angle;
  int rotation;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(60, 29, 100)
  const AVPacketSideData *sd;

  sd = av_packet_side_data_get (stream->codecpar->coded_side_data,
                                stream->codecpar->nb_coded_side_data,
                                AV_PKT_DATA_DISPLAYMATRIX);
  matrix = sd ? (const int32_t *)sd->data : NULL;
#else
  matrix = (const int32_t *)av_stream_get_side_data (
      stream, AV_PKT_DATA_DISPLAYMATRIX, NULL);
#endif
  if (matrix == NULL)
    {
      return 0;
    }

  angle = -av_display_rotation_get (matrix);
  if (isnan (angle))
    {
      return 0;
    }

  rotation = (int)lround (angle / 90) * 90 % 360;
  return rotation < 0 ? rotation + 360 : rotation;
}

static void
video_probe_rotate (const uint8_t *src, uint8_t *dst, int n, int rotation)
{
  int x, y, sx, sy;

  for (y = 0; y < n; ++y)
    {
      for (x = 0; x < n; ++x)
        {
          if (rotation == 90)
            {
              sx = y;
              sy = n - 1 - x;
            }
          else if (rotation == 180)
            {
              sx = n - 1 - x;
              sy = n - 1 - y;
            }
          else
            {
              sx = n - 1 - y;
              sy = x;
            }
          memcpy (dst + (y * n + x) * 3, src + (sy * n + sx) * 3, 3);
        }
    }
}

static int
video_frame_render (const AVFrame *frame, int rotation, int width, int height,
                    uint8_t *const dst[], const int dst_linesize[])
{
  uint8_t *probe, *rotated, *img, *src[1];
  int *rows, *cols;
  int n, x, y, gray, top, bottom, left, right, ret, linesize[1];
  struct SwsContext *ctx;
  const uint8_t *p;

  n = MAX (FDUPVES_VIDEO_PROBE_LEN, MAX (width, height));
  linesize[0] = n * 3;

  ctx = sws_getContext (frame->width, frame->height, frame->format, n, n,
                        AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
  if (ctx == NULL)
    {
      return -1;
    }
  probe = g_malloc (n * n * 3);
  rotated = g_malloc (n * n * 3);
  rows = g_new0 (int, n);
  cols = g_new0 (int, n);
  src[0] = probe;
  sws_scale (ctx, (const uint8_t *const *)frame->data, frame->linesize, 0,
             frame->height, src, linesize);
  sws_freeContext (ctx);

  img = probe;
  if (rotation != 0)
    {
      video_probe_rotate (probe, rotated, n, rotation);
      img = rotated;
    }

  for (y = 0; y < n; ++y)
    {
      for (x = 0; x < n; ++x)
        {
          p = img + (y * n + x) * 3;
          gray = (p[0] * 299 + p[1] * 587 + p[2] * 114) / 1000;
          rows[y] += gray;
          cols[x] += gray;
        }
    }

  top = 0;
  bottom = n;
  while (bottom - top > n / 2 && rows[top] < FDUPVES_VIDEO_BLACK * n)
    ++top;
  while (bottom - top > n / 2 && rows[bottom - 1] < FDUPVES_VIDEO_BLACK * n)
    --bottom;
  left = 0;
  right = n;
  while (right - left > n / 2 && cols[left] < FDUPVES_VIDEO_BLACK * n)
    ++left;
  while (right - left > n / 2 && cols[right - 1] < FDUPVES_VIDEO_BLACK * n)
    --right;

  ret = -1;
  ctx = sws_getContext (right - left, bottom - top, AV_PIX_FMT_RGB24, width,
                        height, AV_PIX_FMT_RGB24, SWS_AREA, NULL, NULL, NULL);
  if (ctx != NULL)
    {
      src[0] = img + (top * n + left) * 3;
      sws_scale (ctx, (const uint8_t *const *)src, linesize, 0, bottom - top,
                 dst, dst_linesize);
      sws_freeContext (ctx);
      ret = 0;
    }

  g_free (cols);
  g_free (rows);
  g_free (rotated);
  g_free (probe);

  return ret;
}

static void
video_close_decoder (video_t *video)
{
  av_packet_free (&video->packet);
  av_frame_free (&video->next);
  av_frame_free (&video->frame);
  avcodec_free_context (&video->codec_ctx);
}

static int
video_open_decoder (video_t *video)
{
  const AVCodec *codec;

  codec = avcodec_find_decoder (video->stream->codecpar->codec_id);
  if (codec == NULL)
    {
      g_warning (_ ("Unsupported codec: %s"), video->file);
      return -1;
    }

  video->codec_ctx = avcodec_alloc_context3 (codec);
  if (video->codec_ctx == NULL
      || avcodec_parameters_to_context (video->codec_ctx,
                                        video->stream->codecpar)
             < 0)
    {
      g_warning (_ ("Memory error: %s"), video->file);
      video_close_decoder (video);
      return -1;
    }

  video->codec_ctx->pkt_timebase = video->stream->time_base;
  // av_codec_set_pkt_timebase (codec_ctx, format_ctx->streams[s]->time_base);

  if (avcodec_open2 (video->codec_ctx, codec, NULL) < 0)
    {
      g_warning (_ ("Open codec error: %s"), video->file);
      video_close_decoder (video);
      return -1;
    }

  video->frame = av_frame_alloc ();
  video->next = av_frame_alloc ();
  video->packet = av_packet_alloc ();
  if (video->frame == NULL || video->next == NULL || video->packet == NULL)
    {
      video_close_decoder (video);
      return -1;
    }

  return 0;
}

int
video_screenshot (video_t *video, double time, int width, int height,
                  char *buffer, int buf_len)
{
  AVStream *stream;
  uint8_t *data[4];
  int linesize[4];
  int ret, bytes, decoded, reached;
  int64_t seek_target;

  if (video->broken)
    {
      return -1;
    }
  if (video->packet == NULL && video_open_decoder (video) < 0)
    {
      video->broken = TRUE;
      return -1;
    }

  bytes = av_image_fill_arrays (data, linesize, (uint8_t *)buffer,
                                AV_PIX_FMT_RGB24, width, height, 1);
  if (bytes < 0 || buf_len < bytes)
    {
      return -1;
    }

  stream = video->stream;
  seek_target = (int64_t)llround (time * stream->time_base.den
                                  / stream->time_base.num);
  if (stream->start_time != AV_NOPTS_VALUE)
    {
      seek_target += stream->start_time;
    }
  av_seek_frame (video->format_ctx, stream->index, seek_target,
                 AVSEEK_FLAG_BACKWARD);
  avcodec_flush_buffers (video->codec_ctx);

  decoded = 0;
  reached = 0;
  while (!reached)
    {
      ret = av_read_frame (video->format_ctx, video->packet);
      if (ret < 0)
        {
          avcodec_send_packet (video->codec_ctx, NULL);
        }
      else if (video->packet->stream_index != stream->index)
        {
          av_packet_unref (video->packet);
          continue;
        }
      else
        {
          avcodec_send_packet (video->codec_ctx, video->packet);
          av_packet_unref (video->packet);
        }

      while (!reached
             && avcodec_receive_frame (video->codec_ctx, video->next) == 0)
        {
          av_frame_unref (video->frame);
          av_frame_move_ref (video->frame, video->next);
          decoded = 1;
          reached = video->frame->best_effort_timestamp == AV_NOPTS_VALUE
                    || video->frame->best_effort_timestamp >= seek_target;
        }

      if (ret < 0)
        {
          break;
        }
    }

  if (!decoded
      || video_frame_render (video->frame, video_stream_rotation (stream),
                             width, height, data, linesize)
             < 0)
    {
      return -1;
    }

  return bytes;
}

void
video_close (video_t *video)
{
  video_close_decoder (video);
  avformat_close_input (&video->format_ctx);
  g_free (video);
}

int
video_time_screenshot (const char *file, double time, int width, int height,
                       char *buffer, int buf_len)
{
  video_t *video;
  double length;
  int ret;

  video = video_open (file, &length);
  if (video == NULL)
    {
      return -1;
    }

  ret = video_screenshot (video, time, width, height, buffer, buf_len);
  video_close (video);

  return ret;
}

int
video_time_screenshot_file (const char *file, double time, int width,
                            int height, const char *out_file)
{
  char *buf;
  int len;
  GdkPixbuf *pix;
  GError *err;

  buf = g_malloc (width * height * 3);
  g_return_val_if_fail (buf, -1);

  len = video_time_screenshot (file, time, width, height, buf,
                               width * height * 3);
  if (len <= 0)
    {
      g_free (buf);
      return -1;
    }

  pix = gdk_pixbuf_new_from_data ((guchar *)buf, GDK_COLORSPACE_RGB, FALSE, 8,
                                  width, height, width * 3, NULL, NULL);
  if (pix)
    {
      err = NULL;
      gdk_pixbuf_save (pix, out_file, "jpeg", &err, "quality", "100", NULL);
      g_object_unref (pix);
      if (err)
        {
          g_free (buf);
          g_warning ("%s: %s", file, err->message);
          g_error_free (err);
          return -1;
        }
    }
  g_free (buf);

  return 0;
}
