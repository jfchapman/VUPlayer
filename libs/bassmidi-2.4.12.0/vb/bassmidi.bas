Attribute VB_Name = "BASSMIDI"
' BASSMIDI 2.4 Visual Basic module
' Copyright (c) 2006-2018 Un4seen Developments Ltd.
'
' See the BASSMIDI.CHM file for more detailed documentation

' Additional error codes returned by BASS_ErrorGetCode
Global Const BASS_ERROR_MIDI_INCLUDE = 7000 ' SFZ include file could not be opened

' Additional BASS_SetConfig options
Global Const BASS_CONFIG_MIDI_COMPACT = &H10400
Global Const BASS_CONFIG_MIDI_VOICES = &H10401
Global Const BASS_CONFIG_MIDI_AUTOFONT = &H10402
Global Const BASS_CONFIG_MIDI_IN_PORTS = &H10404

' Additional BASS_SetConfigPtr options
Global Const BASS_CONFIG_MIDI_DEFFONT = &H10403

' Additional sync types
Global Const BASS_SYNC_MIDI_MARK = &H10000
Global Const BASS_SYNC_MIDI_MARKER = &H10000
Global Const BASS_SYNC_MIDI_CUE = &H10001
Global Const BASS_SYNC_MIDI_LYRIC = &H10002
Global Const BASS_SYNC_MIDI_TEXT = &H10003
Global Const BASS_SYNC_MIDI_EVENT = &H10004
Global Const BASS_SYNC_MIDI_TICK = &H10005
Global Const BASS_SYNC_MIDI_TIMESIG = &H10006
Global Const BASS_SYNC_MIDI_KEYSIG = &H10007

' Additional BASS_MIDI_StreamCreateFile/etc flags
Global Const BASS_MIDI_NOSYSRESET = &H800
Global Const BASS_MIDI_DECAYEND = &H1000
Global Const BASS_MIDI_NOFX = &H2000
Global Const BASS_MIDI_DECAYSEEK = &H4000
Global Const BASS_MIDI_NOCROP = &H8000
Global Const BASS_MIDI_NOTEOFF1 = &H10000
Global Const BASS_MIDI_SINCINTER = &H800000

' BASS_MIDI_FontInit flags
Global Const BASS_MIDI_FONT_MEM = &H10000
Global Const BASS_MIDI_FONT_MMAP = &H20000
Global Const BASS_MIDI_FONT_XGDRUMS = &H40000
Global Const BASS_MIDI_FONT_NOFX = &H80000
Global Const BASS_MIDI_FONT_LINATTMOD = &H100000

Type BASS_MIDI_FONT
    font As Long            ' soundfont
    preset As Long          ' preset number (-1=all)
    bank As Long
End Type

Type BASS_MIDI_FONTEX
    font As Long            ' soundfont
    spreset As Long         ' source preset number
    sbank As Long           ' source bank number
    dpreset As Long         ' destination preset/program number
    dbank As Long           ' destination bank number
    dbanklsb As Long        ' destination bank number LSB
End Type

' BASS_MIDI_StreamSet/GetFonts flag
Global Const BASS_MIDI_FONT_EX = &H1000000 ' BASS_MIDI_FONTEX

Type BASS_MIDI_FONTINFO
    name As Long
    copyright As Long
    comment As Long
    presets As Long         ' number of presets/instruments
    samsize As Long         ' total size (in bytes) of the sample data
    samload As Long         ' amount of sample data currently loaded
    samtype As Long         ' sample format (CTYPE) if packed
End Type

Type BASS_MIDI_MARK
    track As Long           ' track containing marker
    pos As Long             ' marker position
    text As Long            ' marker text
End Type

' Marker types
Global Const BASS_MIDI_MARK_MARKER = 0  ' marker
Global Const BASS_MIDI_MARK_CUE = 1     ' cue point
Global Const BASS_MIDI_MARK_LYRIC = 2   ' lyric
Global Const BASS_MIDI_MARK_TEXT = 3    ' text
Global Const BASS_MIDI_MARK_TIMESIG = 4 ' time signature
Global Const BASS_MIDI_MARK_KEYSIG = 5  ' key signature
Global Const BASS_MIDI_MARK_COPY = 6    ' copyright notice
Global Const BASS_MIDI_MARK_TRACK = 7   ' track name
Global Const BASS_MIDI_MARK_INST = 8    ' instrument name
Global Const BASS_MIDI_MARK_TRACKSTART = 9 ' track start (SMF2)
Global Const BASS_MIDI_MARK_TICK = &H10000 ' FLAG: get position in ticks (otherwise bytes)

' MIDI events
Global Const MIDI_EVENT_NOTE = 1
Global Const MIDI_EVENT_PROGRAM = 2
Global Const MIDI_EVENT_CHANPRES = 3
Global Const MIDI_EVENT_PITCH = 4
Global Const MIDI_EVENT_PITCHRANGE = 5
Global Const MIDI_EVENT_DRUMS = 6
Global Const MIDI_EVENT_FINETUNE = 7
Global Const MIDI_EVENT_COARSETUNE = 8
Global Const MIDI_EVENT_MASTERVOL = 9
Global Const MIDI_EVENT_BANK = 10
Global Const MIDI_EVENT_MODULATION = 11
Global Const MIDI_EVENT_VOLUME = 12
Global Const MIDI_EVENT_PAN = 13
Global Const MIDI_EVENT_EXPRESSION = 14
Global Const MIDI_EVENT_SUSTAIN = 15
Global Const MIDI_EVENT_SOUNDOFF = 16
Global Const MIDI_EVENT_RESET = 17
Global Const MIDI_EVENT_NOTESOFF = 18
Global Const MIDI_EVENT_PORTAMENTO = 19
Global Const MIDI_EVENT_PORTATIME = 20
Global Const MIDI_EVENT_PORTANOTE = 21
Global Const MIDI_EVENT_MODE = 22
Global Const MIDI_EVENT_REVERB = 23
Global Const MIDI_EVENT_CHORUS = 24
Global Const MIDI_EVENT_CUTOFF = 25
Global Const MIDI_EVENT_RESONANCE = 26
Global Const MIDI_EVENT_RELEASE = 27
Global Const MIDI_EVENT_ATTACK = 28
Global Const MIDI_EVENT_DECAY = 29
Global Const MIDI_EVENT_REVERB_MACRO = 30
Global Const MIDI_EVENT_CHORUS_MACRO = 31
Global Const MIDI_EVENT_REVERB_TIME = 32
Global Const MIDI_EVENT_REVERB_DELAY = 33
Global Const MIDI_EVENT_REVERB_LOCUTOFF = 34
Global Const MIDI_EVENT_REVERB_HICUTOFF = 35
Global Const MIDI_EVENT_REVERB_LEVEL = 36
Global Const MIDI_EVENT_CHORUS_DELAY = 37
Global Const MIDI_EVENT_CHORUS_DEPTH = 38
Global Const MIDI_EVENT_CHORUS_RATE = 39
Global Const MIDI_EVENT_CHORUS_FEEDBACK = 40
Global Const MIDI_EVENT_CHORUS_LEVEL = 41
Global Const MIDI_EVENT_CHORUS_REVERB = 42
Global Const MIDI_EVENT_USERFX = 43
Global Const MIDI_EVENT_USERFX_LEVEL = 44
Global Const MIDI_EVENT_USERFX_REVERB = 45
Global Const MIDI_EVENT_USERFX_CHORUS = 46
Global Const MIDI_EVENT_DRUM_FINETUNE = 50
Global Const MIDI_EVENT_DRUM_COARSETUNE = 51
Global Const MIDI_EVENT_DRUM_PAN = 52
Global Const MIDI_EVENT_DRUM_REVERB = 53
Global Const MIDI_EVENT_DRUM_CHORUS = 54
Global Const MIDI_EVENT_DRUM_CUTOFF = 55
Global Const MIDI_EVENT_DRUM_RESONANCE = 56
Global Const MIDI_EVENT_DRUM_LEVEL = 57
Global Const MIDI_EVENT_DRUM_USERFX = 58
Global Const MIDI_EVENT_SOFT = 60
Global Const MIDI_EVENT_SYSTEM = 61
Global Const MIDI_EVENT_TEMPO = 62
Global Const MIDI_EVENT_SCALETUNING = 63
Global Const MIDI_EVENT_CONTROL = 64
Global Const MIDI_EVENT_CHANPRES_VIBRATO = 65
Global Const MIDI_EVENT_CHANPRES_PITCH = 66
Global Const MIDI_EVENT_CHANPRES_FILTER = 67
Global Const MIDI_EVENT_CHANPRES_VOLUME = 68
Global Const MIDI_EVENT_MOD_VIBRATO = 69
Global Const MIDI_EVENT_MODRANGE = 69
Global Const MIDI_EVENT_BANK_LSB = 70
Global Const MIDI_EVENT_KEYPRES = 71
Global Const MIDI_EVENT_KEYPRES_VIBRATO = 72
Global Const MIDI_EVENT_KEYPRES_PITCH = 73
Global Const MIDI_EVENT_KEYPRES_FILTER = 74
Global Const MIDI_EVENT_KEYPRES_VOLUME = 75
Global Const MIDI_EVENT_SOSTENUTO = 76
Global Const MIDI_EVENT_MOD_PITCH = 77
Global Const MIDI_EVENT_MOD_FILTER = 78
Global Const MIDI_EVENT_MOD_VOLUME = 79
Global Const MIDI_EVENT_MIXLEVEL = &H10000
Global Const MIDI_EVENT_TRANSPOSE = &H10001
Global Const MIDI_EVENT_SYSTEMEX = &H10002
Global Const MIDI_EVENT_SPEED = &H10004

Global Const MIDI_EVENT_END = 0
Global Const MIDI_EVENT_END_TRACK = &H10003

Global Const MIDI_EVENT_NOTES = &H20000
Global Const MIDI_EVENT_VOICES = &H20001

Global Const MIDI_SYSTEM_DEFAULT = 0
Global Const MIDI_SYSTEM_GM1 = 1
Global Const MIDI_SYSTEM_GM2 = 2
Global Const MIDI_SYSTEM_XG = 3
Global Const MIDI_SYSTEM_GS = 4

Type BASS_MIDI_EVENT
	event_ As Long          ' MIDI_EVENT_xxx
	param As Long
	chan As Long
	tick As Long            ' event position (ticks)
	pos As Long             ' event position (bytes)
End Type

' BASS_MIDI_StreamEvents modes
Global Const BASS_MIDI_EVENTS_STRUCT = 0     ' BASS_MIDI_EVENT structures
Global Const BASS_MIDI_EVENTS_RAW = &H10000  ' raw MIDI event data
Global Const BASS_MIDI_EVENTS_SYNC = &H1000000 ' FLAG: trigger event syncs
Global Const BASS_MIDI_EVENTS_NORSTATUS = &H2000000 ' FLAG: no running status
Global Const BASS_MIDI_EVENTS_CANCEL = &H4000000 ' flag: cancel pending events
Global Const BASS_MIDI_EVENTS_TIME = &H8000000 ' flag: delta-time info is present
Global Const BASS_MIDI_EVENTS_ABSTIME = &H10000000 ' flag: absolute time info is present

' BASS_MIDI_StreamGetChannel special channels
Global Const BASS_MIDI_CHAN_CHORUS = -1
Global Const BASS_MIDI_CHAN_REVERB = -2
Global Const BASS_MIDI_CHAN_USERFX = -3

' BASS_CHANNELINFO type
Global Const BASS_CTYPE_STREAM_MIDI = &H10D00

' Additional attributes
Global Const BASS_ATTRIB_MIDI_PPQN = &H12000
Global Const BASS_ATTRIB_MIDI_CPU = &H12001
Global Const BASS_ATTRIB_MIDI_CHANS = &H12002
Global Const BASS_ATTRIB_MIDI_VOICES = &H12003
Global Const BASS_ATTRIB_MIDI_VOICES_ACTIVE = &H12004
Global Const BASS_ATTRIB_MIDI_STATE = &H12005
Global Const BASS_ATTRIB_MIDI_SRC = &H12006
Global Const BASS_ATTRIB_MIDI_KILL = &H12007
Global Const BASS_ATTRIB_MIDI_TRACK_VOL = &H12100 ' + track #

' Additional BASS_ChannelGetTags type
Global Const BASS_TAG_MIDI_TRACK = &H11000 ' + track #, track text : array of null-terminated ANSI strings

' BASS_ChannelGetLength/GetPosition/SetPosition mode
Global Const BASS_POS_MIDI_TICK = 2 ' tick position

' BASS_MIDI_FontPack flags
Global Const BASS_MIDI_PACK_NOHEAD = 1 ' don't send a WAV header to the encoder
Global Const BASS_MIDI_PACK_16BIT = 2 ' discard low 8 bits of 24-bit sample data

Type BASS_MIDI_DEVICEINFO
	name As Long	' description
	id As Long
	flags As Long
End Type

Declare Function BASS_MIDI_StreamCreate Lib "bassmidi.dll" (ByVal channels As Long, ByVal flags As Long, ByVal freq As Long) As Long
Declare Function BASS_MIDI_StreamCreateFile64 Lib "bassmidi.dll" Alias "BASS_MIDI_StreamCreateFile" (ByVal mem As Long, ByVal file As Any, ByVal offset As Long, ByVal offsethi As Long, ByVal length As Long, ByVal lengthhi As Long, ByVal flags As Long, ByVal freq As Long) As Long
Declare Function BASS_MIDI_StreamCreateURL Lib "bassmidi.dll" (ByVal url As String, ByVal offset As Long, ByVal flags As Long, ByVal proc As Long, ByVal user As Long, ByVal freq As Long) As Long
Declare Function BASS_MIDI_StreamCreateFileUser Lib "bassmidi.dll" (ByVal system As Long, ByVal flags As Long, ByVal procs As Long, ByVal user As Long, ByVal freq As Long) As Long
Declare Function BASS_MIDI_StreamCreateEvents Lib "bassmidi.dll" (ByRef events As BASS_MIDI_EVENT, ByVal ppqn As Long, ByVal flags As Long, ByVal freq As Long) As Long
Declare Function BASS_MIDI_StreamGetMark Lib "bassmidi.dll" (ByVal handle As Long, ByVal type_ As Long, ByVal index As Long, mark As Any) As Long
Declare Function BASS_MIDI_StreamGetMarks Lib "bassmidi.dll" (ByVal handle As Long, ByVal track As Long, ByVal type_ As Long, marks As Any) As Long
Declare Function BASS_MIDI_StreamSetFonts Lib "bassmidi.dll" (ByVal handle As Long, fonts As Any, ByVal count As Long) As Long
Declare Function BASS_MIDI_StreamGetFonts Lib "bassmidi.dll" (ByVal handle As Long, fonts As Any, ByVal count As Long) As Long
Declare Function BASS_MIDI_StreamLoadSamples Lib "bassmidi.dll" (ByVal handle As Long) As Long
Declare Function BASS_MIDI_StreamEvent Lib "bassmidi.dll" (ByVal handle As Long, ByVal chan As Long, ByVal event_ As Long, ByVal param As Long) As Long
Declare Function BASS_MIDI_StreamEvents Lib "bassmidi.dll" (ByVal handle As Long, ByVal mode As Long, ByVal events As Long, ByVal length As Long) As Long
Declare Function BASS_MIDI_StreamGetEvent Lib "bassmidi.dll" (ByVal handle As Long, ByVal chan As Long, ByVal event_ As Long) As Long
Declare Function BASS_MIDI_StreamGetEvents Lib "bassmidi.dll" (ByVal handle As Long, ByVal track As Long, ByVal filter As Long, events As Any) As Long
Declare Function BASS_MIDI_StreamGetEventsEx Lib "bassmidi.dll" (ByVal handle As Long, ByVal track As Long, ByVal filter As Long, ByVal events As Any, ByVal start As Long, ByVal count As Long) As Long
Declare Function BASS_MIDI_StreamGetPreset Lib "bassmidi.dll" (ByVal handle As Long, ByVal chan As Long, ByRef font As BASS_MIDI_FONT) As Long
Declare Function BASS_MIDI_StreamGetChannel Lib "bassmidi.dll" (ByVal handle As Long, ByVal chan As Long) As Long
Declare Function BASS_MIDI_StreamSetFilter Lib "bassmidi.dll" (ByVal handle As Long, ByVal seeking As Long, ByVal proc As Long, ByVal user As Long) As Long

Declare Function BASS_MIDI_FontInit Lib "bassmidi.dll" (ByVal file As Any, ByVal flags As Long) As Long
Declare Function BASS_MIDI_FontInitUser Lib "bassmidi.dll" (ByVal procs As Long, ByVal user As Long, ByVal flags As Long) As Long
Declare Function BASS_MIDI_FontFree Lib "bassmidi.dll" (ByVal handle As Long) As Long
Declare Function BASS_MIDI_FontGetInfo Lib "bassmidi.dll" (ByVal handle As Long, ByRef info As BASS_MIDI_FONTINFO) As Long
Declare Function BASS_MIDI_FontGetPresets Lib "bassmidi.dll" (ByVal handle As Long, ByRef presets As Long) As Long
Declare Function BASS_MIDI_FontGetPreset Lib "bassmidi.dll" (ByVal handle As Long, ByVal preset As Long, ByVal bank As Long) As Long
Declare Function BASS_MIDI_FontLoad Lib "bassmidi.dll" (ByVal handle As Long, ByVal preset As Long, ByVal bank As Long) As Long
Declare Function BASS_MIDI_FontUnload Lib "bassmidi.dll" (ByVal handle As Long, ByVal preset As Long, ByVal bank As Long) As Long
Declare Function BASS_MIDI_FontCompact Lib "bassmidi.dll" (ByVal handle As Long) As Long
Declare Function BASS_MIDI_FontPack Lib "bassmidi.dll" (ByVal handle As Long, ByVal outfile As String, ByVal encoder As String, ByVal flags As Long) As Long
Declare Function BASS_MIDI_FontUnpack Lib "bassmidi.dll" (ByVal handle As Long, ByVal outfile As String, ByVal flags As Long) As Long
Declare Function BASS_MIDI_FontSetVolume Lib "bassmidi.dll" (ByVal handle As Long, ByVal handle As Single) As Long
Declare Function BASS_MIDI_FontGetVolume Lib "bassmidi.dll" (ByVal handle As Long) As Single

Declare Function BASS_MIDI_ConvertEvents Lib "bassmidi.dll" (ByVal data As Long, ByVal length As Long, events As Any, ByVal count As Long, ByVal flags As Long) As Long

Declare Function BASS_MIDI_InGetDeviceInfo Lib "bassmidi.dll" (ByVal device As Long, ByRef info As BASS_MIDI_DEVICEINFO) As Long
Declare Function BASS_MIDI_InInit Lib "bassmidi.dll" (ByVal device As Long, ByVal proc As Long, ByVal user As Long) As Long
Declare Function BASS_MIDI_InFree Lib "bassmidi.dll" (ByVal device As Long) As Long
Declare Function BASS_MIDI_InStart Lib "bassmidi.dll" (ByVal device As Long) As Long
Declare Function BASS_MIDI_InStop Lib "bassmidi.dll" (ByVal device As Long) As Long

' 32-bit wrappers for 64-bit BASS functions
Function BASS_MIDI_StreamCreateFile(ByVal mem As Long, ByVal file As Long, ByVal offset As Long, ByVal length As Long, ByVal flags As Long, ByVal freq As Long) As Long
BASS_MIDI_StreamCreateFile = BASS_MIDI_StreamCreateFile64(mem, file, offset, 0, length, 0, flags Or BASS_UNICODE, freq)
End Function

' callback functions
Function MIDIFILTERPROC(ByVal handle As Long, ByVal track As Long, ByRef event_ As BASS_MIDI_EVENT, ByVal seeking As Long, ByVal user As Long) As Long

    'CALLBACK FUNCTION !!!
    
    ' Event filtering callback function.
    ' handle : MIDI stream handle
    ' track  : Track containing the event
    ' event_ : The event
    ' seeking: BASSTRUE = the event is being processed while seeking, BASSFALSE = it is being played
    ' user   : The 'user' parameter value given when calling BASS_MIDI_StreamSetFilter
    ' RETURN : BASSTRUE = process the event, BASSFALSE = drop the event

End Function

Sub MIDIINPROC(ByVal device As Long, ByVal time As Double, ByVal buffer As Long, ByVal length As Long, ByVal user As Long)
    
    'CALLBACK FUNCTION !!!
    
    ' MIDI input callback function
    ' device : MIDI input device
    ' time   : Timestamp
    ' buffer : Buffer containing MIDI data
    ' length : Number of bytes of data
    ' user   : The 'user' parameter value given when calling BASS_MIDI_InInit
    
End Sub
