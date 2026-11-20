///
/// Read media file metadata using gstreamer.
/// @file       gstreammetadata.cpp - pianod2
/// @author     Perette Barella
/// @date       2016-08-22
/// @copyright  Copyright © 2016-2023 Devious Fish. All rights reserved.
///

#include <config.h>



#include <string>
#include <memory>

#include "metadata/metadata.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
#pragma GCC diagnostic ignored "-Wshadow"
#include <gst/gst.h>
#pragma GCC diagnostic pop

#include "fundamentals.h"
#include "logging.h"

// This is hacky, but we need the base class of the audio player
#include "../audio/gstreamplayer.h"
#include "gstreammetadata.h"


namespace Media {
    static const char *media_formats[] = {
        ".mp3",
        ".m4a",
        ".flac",
        ".wav",
        ".mp4a",
        ".wma",
        ".snd",
        ".aiff",
        ".aac",
        ".ogg",
        ".mpa",
        ".alac",
        nullptr
    };


    GstreamerMetadataReader::GstreamerMetadataReader (const std::string &path) :
    Audio::GstreamerMediaReader (path, 5) {
        // Gstreamer (or is it libmad?) hasn't proven trustworthy with non-audio files.
        // Ignore anything that doesn't look sane.
        const char **extension;
        for (extension = media_formats; *extension; extension++) {
            int len = strlen (*extension);
            if (path.length() >= len) {
                if (strcasecmp (*extension, path.c_str() + path.length() - len) == 0) {
                    break;
                }
            }
        };
        if (!*extension) {
            throw MediaException ("Not obviously an audio file");
        }

        GstElement *fake_sink = createElement ("fakesink");
        push (fake_sink);

        setPipelineState (GST_STATE_PAUSED);

        bool keep_going = true;
        while (keep_going) {
            auto message = gst_bus_timed_pop (GST_ELEMENT_BUS (pipeline), 5E9);
            keep_going = notification (message);
            gst_message_unref (message);
        }
        setPipelineState (GST_STATE_NULL);
    }


    /** Retrieve a value out of the tag list.
        @param tag_list The tag collection to extract a value from.
        @param tag The tag whose value to retrieve.
        @param type The expected datatype of the tag's value.
        @return If found an the expected type, the value, otherwise null. */
    static const GValue *retrieveTag (const GstTagList *tag_list, const gchar *tag, GType type) {
        auto count = gst_tag_list_get_tag_size (tag_list, tag);
        if (count > 0) {
            const GValue *val = gst_tag_list_get_value_index (tag_list, tag, 0);
            if (G_VALUE_TYPE (val) == type) {
                if (count > 1) {
                    flog (LOG_WHERE (Log::WARNING|Log::METADATA), "Tag ", tag, " has ", count, " values");
                }
                return val;
            }
            flog (LOG_WHERE (Log::WARNING|Log::METADATA), "Tag ", tag, " contains ", G_VALUE_TYPE_NAME (val),
                  " instead of ", g_type_name (type));
        }
        return nullptr;
    }

    /** Extract a floating point value from the tag list.
        @param list A dictionary of tags and values.
        @param tag The tag whose value to retrieve.
        @param value The variable to retrieve into. */
    static void retrieveTag (const GstTagList *list, const gchar *tag, float *value) {
        const GValue *val = retrieveTag (list, tag, G_TYPE_DOUBLE);
        if (val) {
            *value = g_value_get_double (val);
        }
    }

    /** Extract an unsigned integer value from the tag list.
        @param list A dictionary of tags and values.
        @param tag The tag whose value to retrieve.
        @param value The variable to retrieve into. */
    static void retrieveUintTag (const GstTagList *list, const gchar *tag, int *value) {
        const GValue *val = retrieveTag (list, tag, G_TYPE_UINT);
        if (val) {
            *value = g_value_get_uint (val);
        }
    }

    /** Extract the year from the tag list.
        @param list A dictionary of tags and values.
        @param tag The tag whose value to retrieve.
        @param value The variable to retrieve into. */
    static void retrieveYearTag (const GstTagList *list, const gchar *tag, int *value) {
        auto count = gst_tag_list_get_tag_size (list, tag);
        if (count > 0) {
            if (count > 1) {
                flog (LOG_WHERE (Log::WARNING), "Tag ", tag, " has ", count, " values");
            }
            const GValue *val = retrieveTag (list, tag, G_TYPE_DATE);
            if (val) {
                GDate *date = nullptr;
                kept_assert (gst_tag_list_get_date_index (list, tag, 0, &date));
                *value = g_date_get_year (date);
                g_date_free (date);
            }
        }
    }

    /** Extract a string value from the tag list.
        @param list A dictionary of tags and values.
        @param tag The tag whose value to retrieve.
        @param value The variable to retrieve into. */
    static void retrieveDurationTag (const GstTagList *list, const gchar *tag, float *value) {
        const GValue *val = retrieveTag (list, tag, G_TYPE_UINT64);
        if (val) {
            *value = g_value_get_uint64 (val) / 1E9;
        }
    }

    /** Extract a string value from the tag list.
        @param list A dictionary of tags and values.
        @param tag The tag whose value to retrieve.
        @param value The variable to retrieve into. */
    static void retrieveTag (const GstTagList *list, const gchar *tag, std::string *value) {
        const GValue *val = retrieveTag (list, tag, G_TYPE_STRING);
        if (val) {
            *value = g_value_get_string (val);
        }
    }


    bool GstreamerMetadataReader::notification (GstMessage *message) {
        // If all tags have been read, or error encountered, or end of stream,
        // stop processing and shutdown the pipeline.
        GstMessageType msg_type = GST_MESSAGE_TYPE (message);
        if (msg_type == GST_MESSAGE_ASYNC_DONE ||
            msg_type == GST_MESSAGE_ERROR ||
            msg_type == GST_MESSAGE_EOS) {
            // Need to initiate pipeline shutdown before ringing the notifier
            // to prevent race conditions.
            return false;
        }

        // Ignore anything not metadata-related.
        if (msg_type != GST_MESSAGE_TAG)
            return true;

        // Extract meta tags & values.
        GstTagList *tag_list = nullptr;
        gst_message_parse_tag (message, &tag_list);
        if (tag_list) {
            retrieveTag (tag_list, GST_TAG_ARTIST, &artist);
            retrieveTag (tag_list, GST_TAG_ALBUM, &album);
            retrieveTag (tag_list, GST_TAG_TITLE, &title);
            // retrieveTag (tag_list, Gst::TAG, &cddb_id);
            retrieveTag (tag_list, GST_TAG_GENRE, &genre);

            retrieveYearTag (tag_list, GST_TAG_DATE, &year);
            retrieveUintTag (tag_list, GST_TAG_TRACK_NUMBER, &track_number);
            retrieveUintTag (tag_list, GST_TAG_TRACK_COUNT, &track_count);
            retrieveUintTag (tag_list, GST_TAG_ALBUM_VOLUME_NUMBER, &disc_number);
            retrieveUintTag (tag_list, GST_TAG_ALBUM_VOLUME_COUNT, &disc_count);
            retrieveDurationTag (tag_list, GST_TAG_DURATION, &duration);
            retrieveTag (tag_list, GST_TAG_REFERENCE_LEVEL, &gain);
        } else {
            flog (LOG_WHERE (Log::ERROR), "gst_message_parse_tag: provided null tag list.");
        }

        gst_tag_list_unref(tag_list);
        return true;
    }

}
