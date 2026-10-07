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

video_info *
video_get_info (const char *file)
{
  video_info *info;
  AVFormatContext *fmt_ctx = NULL;
  AVStream *stream = NULL;
  int s, ret;

  ret = avformat_open_input (&fmt_ctx, file, NULL, NULL);
  if (ret != 0)
    {
      g_warning (_ ("could not open: %s"), file);
      return NULL;
    }

  /*
  if (avformat_find_stream_info (fmt_ctx, NULL) < 0)
    {
      g_warning (_ ("could not find stream infomations: %s"), file);
      avformat_close_input (&fmt_ctx);
      return NULL;
    }*/

  s = av_find_best_stream (fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (s < 0)
    {
      g_warning (_ ("could not find video stream: %s"), file);
      avformat_close_input (&fmt_ctx);
      return NULL;
    }

  stream = fmt_ctx->streams[s];

  info = g_malloc0 (sizeof (video_info));

  info->name = g_path_get_basename (file);
  info->dir = g_path_get_dirname (file);
  if (stream->duration != AV_NOPTS_VALUE)
    {
      info->length = (double)(stream->duration * stream->time_base.num)
                     / stream->time_base.den;
    }
  else
    {
      info->length = (double)(fmt_ctx->duration) / AV_TIME_BASE;
    }
  info->size[0] = stream->codecpar->width;
  info->size[1] = stream->codecpar->height;
  info->format = avcodec_get_name (stream->codecpar->codec_id);

  avformat_close_input (&fmt_ctx);

  return info;
}

void
video_info_free (video_info *info)
{
  g_free (info->name);
  g_free (info->dir);
  g_free (info);
}

int
video_get_length (const char *file)
{
  video_info *info;
  int length;

  length = 0;
  info = video_get_info (file);
  if (info)
    {
      length = (int)info->length;
      video_info_free (info);
    }

  return length;
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

int
video_time_screenshot (const char *file, double time, int width, int height,
                       char *buffer, int buf_len)
{
  AVFormatContext *format_ctx = NULL;
  AVCodecContext *codec_ctx = NULL;
  const AVCodec *codec = NULL;
  AVStream *stream;
  AVFrame *frame, *next, *frame_rgb;
  AVPacket *packet;
  int s, ret, bytes, decoded, reached;
  int64_t seek_target;

  if (avformat_open_input (&format_ctx, file, NULL, NULL) != 0)
    {
      g_warning (_ ("could not open: %s"), file);
      return -1;
    }

  /*
  if (avformat_find_stream_info (format_ctx, NULL) < 0)
    {
      g_warning (_ ("could not find stream infomations: %s"), file);
      avformat_close_input (&format_ctx);
      return -1;
    }*/

  s = av_find_best_stream (format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (s < 0)
    {
      g_warning (_ ("could not find video stream: %s"), file);
      avformat_close_input (&format_ctx);
      return -1;
    }
  stream = format_ctx->streams[s];

  codec = avcodec_find_decoder (stream->codecpar->codec_id);
  if (codec == NULL)
    {
      g_warning (_ ("Unsupported codec: %s"), file);
      avcodec_free_context (&codec_ctx);
      avformat_close_input (&format_ctx);
      return -1;
    }

  codec_ctx = avcodec_alloc_context3 (codec);
  if (codec_ctx == NULL)
    {
      g_warning (_ ("Memory error: %s"), file);
      avformat_close_input (&format_ctx);
      return -1;
    }

  ret = avcodec_parameters_to_context (codec_ctx, stream->codecpar);
  if (ret < 0)
    {
      g_warning (_ ("Memory error: %s"), file);
      avcodec_free_context (&codec_ctx);
      avformat_close_input (&format_ctx);
      return -1;
    }

  codec_ctx->pkt_timebase = stream->time_base;
  // av_codec_set_pkt_timebase (codec_ctx, format_ctx->streams[s]->time_base);

  if (avcodec_open2 (codec_ctx, codec, NULL) < 0)
    {
      g_warning (_ ("Open codec error: %s"), file);
      avcodec_free_context (&codec_ctx);
      avformat_close_input (&format_ctx);
      return -1;
    }

  frame = av_frame_alloc ();
  next = av_frame_alloc ();
  frame_rgb = av_frame_alloc ();
  packet = av_packet_alloc ();
  if (frame == NULL || next == NULL || frame_rgb == NULL || packet == NULL)
    {
      av_packet_free (&packet);
      av_frame_free (&frame_rgb);
      av_frame_free (&next);
      av_frame_free (&frame);
      avcodec_free_context (&codec_ctx);
      avformat_close_input (&format_ctx);
      return -1;
    }
  bytes = av_image_fill_arrays (frame_rgb->data, frame_rgb->linesize,
                                (uint8_t *)buffer, AV_PIX_FMT_RGB24, width,
                                height, 1);
  if (buf_len < bytes)
    {
      bytes = -1;
    }

  seek_target = (int64_t)llround (time * stream->time_base.den
                                  / stream->time_base.num);
  if (stream->start_time != AV_NOPTS_VALUE)
    {
      seek_target += stream->start_time;
    }
  av_seek_frame (format_ctx, s, seek_target, AVSEEK_FLAG_BACKWARD);

  decoded = 0;
  reached = 0;
  while (bytes >= 0 && !reached)
    {
      ret = av_read_frame (format_ctx, packet);
      if (ret < 0)
        {
          avcodec_send_packet (codec_ctx, NULL);
        }
      else if (packet->stream_index != s)
        {
          av_packet_unref (packet);
          continue;
        }
      else
        {
          avcodec_send_packet (codec_ctx, packet);
          av_packet_unref (packet);
        }

      while (!reached && avcodec_receive_frame (codec_ctx, next) == 0)
        {
          av_frame_unref (frame);
          av_frame_move_ref (frame, next);
          decoded = 1;
          reached = frame->best_effort_timestamp == AV_NOPTS_VALUE
                    || frame->best_effort_timestamp >= seek_target;
        }

      if (ret < 0)
        {
          break;
        }
    }

  if (!decoded
      || video_frame_render (frame, video_stream_rotation (stream), width,
                             height, frame_rgb->data, frame_rgb->linesize)
             < 0)
    {
      bytes = -1;
    }

  av_packet_free (&packet);
  av_frame_free (&frame_rgb);
  av_frame_free (&next);
  av_frame_free (&frame);

  avcodec_free_context (&codec_ctx);

  avformat_close_input (&format_ctx);

  return bytes;
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
