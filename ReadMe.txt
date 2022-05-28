VUPlayer 4.14
Copyright (c) 2022 James Chapman
------------------------------------------------------------------------------
website: http://www.vuplayer.com
email:   james@vuplayer.com
------------------------------------------------------------------------------

Overview
--------
VUPlayer is an open-source multi-format audio player for Windows 7 SP1 or later.
MusicBrainz functionality requires an internet connection.
Audioscrobbler functionality requires an internet connection and a Last.fm account.


Command-line
------------
Ordinarily, VUPlayer stores application settings and metadata in a SQLite database in the user documents folder.
To run in 'portable' mode, leaving no database footprint, the application can be launched using the following command-line argument(s):

	VUPlayer.exe -portable [settings file]

The optional [settings file] parameter specifies the location of an application settings file to use.
The 'Export Settings' function in the main application can be used to save the current settings in the correct format.
Please note that MusicBrainz & Audioscrobbler functionality is disabled when running in 'portable' mode.


Credits
-------
Main application is copyright (c) 2022 James Chapman
http://www.vuplayer.com

BASS library is copyright (c) 1999-2022 Un4seen Developments Ltd.
http://www.un4seen.com

Ogg Vorbis is copyright (c) 1994-2022 Xiph.Org
http://xiph.org/vorbis/

Opus is copyright (c) 2011-2022 Xiph.Org
https://opus-codec.org/

FLAC is copyright (c) 2000-2009 Josh Coalson, 2011-2022 Xiph.Org
http://xiph.org/flac/

WavPack is copyright (c) 1998-2022 David Bryant
http://www.wavpack.com

Monkey's Audio is copyright (c) 2000-2022 Matt Ashland
https://monkeysaudio.com

Musepack is copyright (c) The Musepack Development Team
https://www.musepack.net

MusicBrainz is copyright (c) The MetaBrainz Foundation
https://musicbrainz.org/

Audioscrobbler is copyright (c) 2022 Last.fm Ltd.
http://www.last.fm

JSON for Modern C++ is copyright (c) 2013-2022 Niels Lohmann
https://github.com/nlohmann/json

This software uses libraries from the FFmpeg project under the LGPLv2.1
https://ffmpeg.org/


License
-------
Copyright (c) 2022 James Chapman

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
