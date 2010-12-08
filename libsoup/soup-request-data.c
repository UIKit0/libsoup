/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-request-data.c: data: URI request object
 *
 * Copyright (C) 2009, 2010 Red Hat, Inc.
 * Copyright (C) 2010 Igalia, S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define LIBSOUP_USE_UNSTABLE_REQUEST_API

#include "soup-request-data.h"

#include "soup-requester.h"
#include <libsoup/soup.h>
#include <glib/gi18n.h>

G_DEFINE_TYPE (SoupRequestData, soup_request_data, SOUP_TYPE_REQUEST)

struct _SoupRequestDataPrivate {
	gsize content_length;
	char *content_type;
};

static void
soup_request_data_init (SoupRequestData *data)
{
	data->priv = G_TYPE_INSTANCE_GET_PRIVATE (data, SOUP_TYPE_REQUEST_DATA, SoupRequestDataPrivate);
}

static void
soup_request_data_finalize (GObject *object)
{
	SoupRequestData *data = SOUP_REQUEST_DATA (object);

	g_free (data->priv->content_type);

	G_OBJECT_CLASS (soup_request_data_parent_class)->finalize (object);
}

static gboolean
soup_request_data_check_uri (SoupRequest  *request,
			     SoupURI      *uri,
			     GError      **error)
{
	return uri->host == NULL;
}

static GInputStream *
soup_request_data_send (SoupRequest   *request,
			GCancellable  *cancellable,
			GError       **error)
{
	SoupRequestData *data = SOUP_REQUEST_DATA (request);
	SoupURI *uri = soup_request_get_uri (request);
	GInputStream *memstream;
	const char *comma, *semi, *start, *end;
	gboolean base64 = FALSE;
	char *uristr;

	uristr = soup_uri_to_string (uri, FALSE);
	comma = strchr (uristr, ',');
	if (comma && comma != uristr) {
		/* Deal with MIME type / params */
		semi = memchr (uristr, ';', comma - uristr);
		end = semi ? semi : comma;

		if (semi && !g_ascii_strncasecmp (semi, ";base64", MAX ((size_t) (comma - semi), strlen (";base64"))))
			base64 = TRUE;

		if (end != uristr) {
			data->priv->content_type = g_strndup (uristr, end - uristr);
			if (!base64)
				soup_uri_decode (data->priv->content_type);
		}
	}

	memstream = g_memory_input_stream_new ();

	start = comma ? comma + 1 : uristr;

	if (*start) {
		guchar *buf;

		if (base64) {
			int inlen, state = 0;
			guint save = 0;

			inlen = strlen (start);
			buf = g_malloc0 (inlen * 3 / 4 + 3);
			data->priv->content_length =
				g_base64_decode_step (start, inlen, buf,
						      &state, &save);
			if (state != 0) {
				g_free (buf);
				goto fail;
			}
		} else {
			soup_uri_decode (start);
			data->priv->content_length = strlen (start);
			buf = g_memdup (start, data->priv->content_length);
		}

		g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (memstream),
						buf, data->priv->content_length,
						g_free);
	}
	g_free (uristr);

	return memstream;

 fail:
	g_free (uristr);
	g_set_error (error, SOUP_REQUESTER_ERROR, SOUP_REQUESTER_ERROR_BAD_URI,
		     _("Unable to decode URI: %s"), start);
	g_object_unref (memstream);
	return NULL;
}

static goffset
soup_request_data_get_content_length (SoupRequest *request)
{
	SoupRequestData *data = SOUP_REQUEST_DATA (request);

	return data->priv->content_length;
}

static const char *
soup_request_data_get_content_type (SoupRequest *request)
{
	SoupRequestData *data = SOUP_REQUEST_DATA (request);

	return data->priv->content_type;
}

static void
soup_request_data_class_init (SoupRequestDataClass *request_data_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (request_data_class);
	SoupRequestClass *request_class =
		SOUP_REQUEST_CLASS (request_data_class);

	g_type_class_add_private (request_data_class, sizeof (SoupRequestDataPrivate));

	object_class->finalize = soup_request_data_finalize;

	request_class->check_uri = soup_request_data_check_uri;
	request_class->send = soup_request_data_send;
	request_class->get_content_length = soup_request_data_get_content_length;
	request_class->get_content_type = soup_request_data_get_content_type;
}
