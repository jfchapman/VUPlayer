VUPlayer 4.22
Copyright (c) 2025 James Chapman
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
To store this database in the application folder, the following command-line argument can be used to run in 'portable' mode:

	VUPlayer.exe -portable

Please note that, when running in 'portable' mode, database storage requires write permission to the application folder.


Credits
-------
Main application is copyright (c) 2025 James Chapman
http://www.vuplayer.com

BASS library is copyright (c) 1999-2025 Un4seen Developments Ltd.
http://www.un4seen.com

Ogg Vorbis is copyright (c) 1994-2023 Xiph.Org
http://xiph.org/vorbis/

Opus is copyright (c) 2011-2024 Xiph.Org
https://opus-codec.org/

FLAC is copyright (c) 2000-2009 Josh Coalson, 2011-2025 Xiph.Org
http://xiph.org/flac/

WavPack is copyright (c) 1998-2025 David Bryant
http://www.wavpack.com

MusicBrainz is copyright (c) The MetaBrainz Foundation
https://musicbrainz.org/

Audioscrobbler is copyright (c) 2025 Last.fm Ltd.
http://www.last.fm

JSON for Modern C++ is copyright (c) 2013-2024 Niels Lohmann
https://github.com/nlohmann/json

This software uses libraries from the FFmpeg project under the LGPLv2.1
https://ffmpeg.org/

This software uses the libopenmpt library under the BSD-3-Clause License
Copyright (c) 2004-2025, OpenMPT Project Developers and Contributors
Copyright (c) 1997-2003, Olivier Lapicque
https://lib.openmpt.org/libopenmpt/


License
-------
Copyright (c) 2025 James Chapman

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
