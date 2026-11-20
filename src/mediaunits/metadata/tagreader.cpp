/* Copyright (C) 2003 Scott Wheeler <wheeler@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <iomanip>
#include <stdio.h>

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>


TagLib::String formatSeconds(int seconds)
{
  char secondsString[3];
  sprintf(secondsString, "%02i", seconds);
  return secondsString;
}

int main(int argc, char *argv[])
{
  for(int i = 1; i < (argc < 2 ? 2 : argc); i++) {
    const char *name = argv [i] ? argv [i] : "../filesystem/cookies.mp3";
    std::cout << "******************** \"" << name << "\" ********************" << std::endl;

    TagLib::FileRef f(name);

    if(!f.isNull() && f.tag()) {

      TagLib::Tag *tag = f.tag();

      std::cout << "-- TAG (basic) --" << std::endl;
      std::cout << "title   - \"" << tag->title()   << "\"" << std::endl;
      std::cout << "artist  - \"" << tag->artist()  << "\"" << std::endl;
      std::cout << "album   - \"" << tag->album()   << "\"" << std::endl;
      std::cout << "year    - \"" << tag->year()    << "\"" << std::endl;
      std::cout << "comment - \"" << tag->comment() << "\"" << std::endl;
      std::cout << "track   - \"" << tag->track()   << "\"" << std::endl;
      std::cout << "genre   - \"" << tag->genre()   << "\"" << std::endl;

// Pianod change: Do it again, using .to8Bit().
// If the libtag compiler and pianod compiler are incompatible,
// then hopefully the resulting tagreader executable dies and
// exposes the problem, rather than problems showing up in pianod.
      std::cout << "-- TAG (basic) --" << std::endl;
      std::cout << "title   - \"" << tag->title().to8Bit(true)   << "\"" << std::endl;
      std::cout << "artist  - \"" << tag->artist().to8Bit(true)  << "\"" << std::endl;
      std::cout << "album   - \"" << tag->album().to8Bit(true)   << "\"" << std::endl;
      std::cout << "year    - \"" << tag->year()                 << "\"" << std::endl;
      std::cout << "comment - \"" << tag->comment().to8Bit(true) << "\"" << std::endl;
      std::cout << "track   - \"" << tag->track()                << "\"" << std::endl;
      std::cout << "genre   - \"" << tag->genre().to8Bit(true)   << "\"" << std::endl;
      TagLib::PropertyMap tags = f.file()->properties();

      unsigned int longest = 0;
      for(TagLib::PropertyMap::ConstIterator i = tags.begin(); i != tags.end(); ++i) {
        if (i->first.size() > longest) {
          longest = i->first.size();
        }
      }

      std::cout << "-- TAG (properties) --" << std::endl;
      for(TagLib::PropertyMap::ConstIterator i = tags.begin(); i != tags.end(); ++i) {
        for(TagLib::StringList::ConstIterator j = i->second.begin(); j != i->second.end(); ++j) {
          std::cout << std::left << std::setw(longest) << i->first << " - " << '"' << *j << '"' << std::endl;
        }
      }

    }

    if(!f.isNull() && f.audioProperties()) {

      TagLib::AudioProperties *properties = f.audioProperties();

      int seconds = properties->length() % 60;
      int minutes = (properties->length() - seconds) / 60;

      std::cout << "-- AUDIO --" << std::endl;
      std::cout << "bitrate     - " << properties->bitrate() << std::endl;
      std::cout << "sample rate - " << properties->sampleRate() << std::endl;
      std::cout << "channels    - " << properties->channels() << std::endl;
      std::cout << "length      - " << minutes << ":" << formatSeconds(seconds) << std::endl;
    }
  }
  return 0;
}
