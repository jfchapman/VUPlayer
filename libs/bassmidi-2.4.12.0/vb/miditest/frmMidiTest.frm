VERSION 5.00
Object = "{F9043C88-F6F2-101A-A3C9-08002B2F49FB}#1.2#0"; "COMDLG32.OCX"
Object = "{831FDD16-0C5C-11D2-A9FC-0000F8754DA1}#2.0#0"; "MSCOMCTL.OCX"
Begin VB.Form frmMidiTest 
   BorderStyle     =   3  'Fixed Dialog
   Caption         =   "BASSMIDI test"
   ClientHeight    =   3465
   ClientLeft      =   45
   ClientTop       =   330
   ClientWidth     =   6615
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MinButton       =   0   'False
   ScaleHeight     =   231
   ScaleMode       =   3  'Pixel
   ScaleWidth      =   441
   StartUpPosition =   2  'CenterScreen
   Begin VB.Frame frameTempo 
      Caption         =   " Tempo "
      Height          =   2655
      Left            =   5640
      TabIndex        =   9
      Top             =   720
      Width           =   855
      Begin MSComctlLib.Slider sldTempo 
         Height          =   2055
         Left            =   240
         TabIndex        =   10
         Top             =   480
         Width           =   435
         _ExtentX        =   767
         _ExtentY        =   3625
         _Version        =   393216
         Orientation     =   1
         Max             =   20
         SelStart        =   10
         TickFrequency   =   10
         Value           =   10
      End
      Begin VB.Label lblBPM 
         Alignment       =   2  'Center
         Height          =   255
         Left            =   120
         TabIndex        =   11
         Top             =   240
         Width           =   615
      End
   End
   Begin VB.CheckBox chkFX 
      Caption         =   "Reverb && Chorus"
      Height          =   255
      Left            =   3960
      TabIndex        =   8
      Top             =   960
      Value           =   1  'Checked
      Width           =   1575
   End
   Begin VB.Timer tmrMidiTest 
      Interval        =   500
      Left            =   120
      Top             =   0
   End
   Begin VB.Frame frameSoundfont 
      Caption         =   " Soundfont "
      Height          =   735
      Left            =   120
      TabIndex        =   4
      Top             =   2640
      Width           =   5415
      Begin VB.CommandButton btnReplace 
         Caption         =   "Replace"
         Height          =   375
         Left            =   4080
         TabIndex        =   5
         Top             =   240
         Width           =   1215
      End
      Begin VB.Label lblSoundfont 
         Height          =   375
         Left            =   120
         TabIndex        =   7
         Top             =   240
         Width           =   3855
      End
   End
   Begin VB.Frame frameLyrics 
      Caption         =   " Lyrics "
      Height          =   975
      Left            =   120
      TabIndex        =   3
      Top             =   1560
      Width           =   5415
      Begin VB.Label lblLyrics 
         Alignment       =   2  'Center
         Height          =   615
         Left            =   120
         TabIndex        =   6
         Top             =   240
         Width           =   5175
      End
   End
   Begin MSComDlg.CommonDialog CMD 
      Left            =   5880
      Top             =   0
      _ExtentX        =   847
      _ExtentY        =   847
      _Version        =   393216
   End
   Begin VB.CommandButton cmdOpenFP 
      Caption         =   "click here to open a file && play it"
      Height          =   375
      Left            =   120
      TabIndex        =   0
      Top             =   120
      Width           =   6375
   End
   Begin MSComctlLib.Slider sldPosition 
      Height          =   675
      Left            =   120
      TabIndex        =   1
      Top             =   840
      Width           =   3735
      _ExtentX        =   6588
      _ExtentY        =   1191
      _Version        =   393216
      LargeChange     =   1
      Max             =   100
      TickStyle       =   2
      TickFrequency   =   0
   End
   Begin VB.Label lblTitle 
      Alignment       =   2  'Center
      Height          =   195
      Left            =   120
      TabIndex        =   2
      Top             =   600
      Width           =   5415
   End
End
Attribute VB_Name = "frmMidiTest"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
'=================================================================================
' frmMidiTest.frm - Copyright (c) 2006-2010 (: JOBnik! :) [Arthur Aminov, ISRAEL]
'                                                         [http://www.jobnik.org]
'                                                         [  jobnik@jobnik.org  ]
' Other sources: modMidiTest.bas
'
' BASSMIDI test
' * Requires: BASS 2.4 (available @ www.un4seen.com)
'=================================================================================

Option Explicit

' display error dialogs
Sub Error_(ByVal es As String)
    Call MsgBox(es & vbCrLf & vbCrLf & "(error code: " & BASS_ErrorGetCode & ")", vbExclamation, "Error")
End Sub

Private Sub chkFX_Click()
    If (chkFX.value = vbChecked) Then
        Call BASS_ChannelFlags(chan, 0, BASS_MIDI_NOFX)   ' enable FX
    Else
        Call BASS_ChannelFlags(chan, BASS_MIDI_NOFX, BASS_MIDI_NOFX)   ' disable FX
    End If
End Sub

Private Sub Form_Initialize()
    ' change and set the current path, to prevent from VB not finding BASS.DLL
    ChDrive App.Path
    ChDir App.Path
    
    ' check the correct BASS was loaded
    If (HiWord(BASS_GetVersion) <> BASSVERSION) Then
        Call MsgBox("An incorrect version of BASS.DLL was loaded", vbCritical)
        End
    End If

    ' setup output - default device, 44100hz, stereo, 16 bits
    If (BASS_Init(-1, 44100, 0, Me.hWnd, 0) = 0) Then
        Call Error_("Can't initialize device")
        End
    End If
    
    ' get default font (28mbgm.sf2/ct8mgm.sf2/ct4mgm.sf2/ct2mgm.sf2 if available)
    Dim sf As BASS_MIDI_FONT
    If (BASS_MIDI_StreamGetFonts(0, sf, 1)) Then font_ = sf.font

    ' load optional plugins for packed soundfonts (others may be used too)
    Call BASS_PluginLoad("bassflac.dll", 0)
    Call BASS_PluginLoad("basswv.dll", 0)

    ' tempo adjustment
    temposcale = 1
End Sub

Private Sub Form_Unload(Cancel As Integer)
    Call BASS_Free      ' free BASS
    Call BASS_PluginFree(0)
End Sub

Private Sub cmdOpenFP_Click()
    On Local Error Resume Next    ' in case Cancel was pressed

    CMD.CancelError = True
    CMD.flags = cdlOFNExplorer Or cdlOFNFileMustExist Or cdlOFNHideReadOnly
    CMD.filter = "MIDI files (mid/midi/rmi/kar)|*.mid;*.midi;*.rmi;*.kar|All files|*.*"
    CMD.ShowOpen

    ' if cancel was pressed, exit the procedure
    If Err.Number = 32755 Then Exit Sub

    Call BASS_StreamFree(chan)  ' free old stream before opening new
    lblLyrics.Caption = ""      ' clear lyrics display

    chan = BASS_MIDI_StreamCreateFile(BASSFALSE, StrPtr(CMD.filename), 0, 0, BASS_SAMPLE_LOOP Or IIf(chkFX.value, 0, BASS_MIDI_NOFX), 1)
    
    ' it ain't a MIDI
    If chan = 0 Then
        cmdOpenFP.Caption = "click here to open a file && play it..."
        lblTitle.Caption = ""
        Call Error_("Can't play the file")
        Exit Sub
    End If
    
    ' update the button to show the loaded file name
    cmdOpenFP.Caption = GetFileName(CMD.filename)
    
    ' set the title (first text in first track)
    lblTitle.Caption = VBStrFromAnsiPtr(BASS_ChannelGetTags(chan, BASS_TAG_MIDI_TRACK))

    ' update pos scroller range (using tick length)
    sldPosition.max = BASS_ChannelGetLength(chan, BASS_POS_MIDI_TICK) / 120
    sldPosition.value = 0

    ' set looping syncs
    Dim mark As BASS_MIDI_MARK
    If (FindMarker(chan, "loopend", mark)) Then ' found a loop end point
        Call BASS_ChannelSetSync(chan, BASS_SYNC_POS Or BASS_SYNC_MIXTIME, mark.pos, AddressOf LoopSync, 0)  ' set a sync there
    End If
    Call BASS_ChannelSetSync(chan, BASS_SYNC_END Or BASS_SYNC_MIXTIME, 0, AddressOf LoopSync, 0) ' set one at the end too (eg. in case of seeking past the loop point)

    ' clear lyrics buffer and set lyrics syncs
    lyrics = ""
    
    If (BASS_MIDI_StreamGetMark(chan, BASS_MIDI_MARK_LYRIC, 0, mark)) Then ' got lyrics
        Call BASS_ChannelSetSync(chan, BASS_SYNC_MIDI_MARK, BASS_MIDI_MARK_LYRIC, AddressOf LyricSync, BASS_MIDI_MARK_LYRIC)
    ElseIf (BASS_MIDI_StreamGetMark(chan, BASS_MIDI_MARK_TEXT, 20, mark)) Then ' got text instead (over 20 of them)
        Call BASS_ChannelSetSync(chan, BASS_SYNC_MIDI_MARK, BASS_MIDI_MARK_TEXT, AddressOf LyricSync, BASS_MIDI_MARK_TEXT)
    End If
    Call BASS_ChannelSetSync(chan, BASS_SYNC_END, 0, AddressOf EndSync, 0)

    ' override the initial tempo, and set a sync to override tempo events and another to override after seeking
    Call SetTempo(True)
    Call BASS_ChannelSetSync(chan, BASS_SYNC_MIDI_EVENT Or BASS_SYNC_MIXTIME, MIDI_EVENT_TEMPO, AddressOf TempoSync, 0)
    Call BASS_ChannelSetSync(chan, BASS_SYNC_SETPOS Or BASS_SYNC_MIXTIME, 0, AddressOf TempoSync, 0)

    ' get default soundfont in case of matching soundfont being used
    Dim sf As BASS_MIDI_FONT
    Call BASS_MIDI_StreamGetFonts(chan, sf, 1)
    font_ = sf.font

    Call BASS_ChannelPlay(chan, BASSFALSE)
End Sub

Private Sub btnReplace_Click()
    On Local Error Resume Next    ' in case Cancel was pressed

    CMD.CancelError = True
    CMD.flags = cdlOFNExplorer Or cdlOFNFileMustExist Or cdlOFNHideReadOnly
    CMD.filter = "Soundfonts (sf2/sf2pack)|*.sf2;*.sf2pack|All files|*.*"
    CMD.ShowOpen

    ' if cancel was pressed, exit the procedure
    If Err.Number = 32755 Then Exit Sub

    Dim newfont As Long
    newfont = BASS_MIDI_FontInit(CMD.filename, 0)
    If (newfont) Then
        Dim sf As BASS_MIDI_FONT
        sf.font = newfont
        sf.preset = -1 ' use all presets
        sf.bank = 0 ' use default bank(s)
        Call BASS_MIDI_StreamSetFonts(0, sf, 1) ' set default soundfont
        Call BASS_MIDI_StreamSetFonts(chan, sf, 1) ' set for current stream too
        Call BASS_MIDI_FontFree(font_) ' free old soundfont
        font_ = newfont
    End If
End Sub

Private Sub sldPosition_MouseDown(Button As Integer, Shift As Integer, X As Single, Y As Single)
    tmrMidiTest.Enabled = False
End Sub

Private Sub sldPosition_MouseUp(Button As Integer, Shift As Integer, X As Single, Y As Single)
    Call BASS_ChannelSetPosition(chan, sldPosition.value * 120, BASS_POS_MIDI_TICK) ' set the position

    lyrics = "" ' clear lyrics
    lblLyrics.Caption = ""
    tmrMidiTest.Enabled = True
End Sub

Private Sub sldTempo_Scroll()
    temposcale = 1 / ((30 - sldTempo.value) / sldTempo.max) ' up to +/- 50% bpm
    Call SetTempo(False)  ' apply the tempo adjustment
    sldTempo.text = (sldTempo.value - sldTempo.max / 2) * -1 * 5 & "%"
End Sub

Private Sub tmrMidiTest_Timer()
    If (chan) Then
        sldPosition.value = BASS_ChannelGetPosition(chan, BASS_POS_MIDI_TICK) / 120 ' update position
        lblBPM.Caption = Format(60000000 / (miditempo * temposcale), ".0") ' calculate bpm
    End If

    Dim text As String
    Static updatefont As Long

    updatefont = updatefont + 1
    If (updatefont And 1) Then ' only updating font info once a second
        text = "no soundfont"

        Dim i As BASS_MIDI_FONTINFO

        If (BASS_MIDI_FontGetInfo(font_, i)) Then
            text = "name: " & VBStrFromAnsiPtr(i.name) & vbCrLf & "loaded: " & i.samload & " / " & i.samsize
        End If

        lblSoundfont = text
        updatefont = 0
    End If
End Sub

'--------------------
' useful function :)
'--------------------

' get file name from file path
Public Function GetFileName(ByVal fp As String) As String
    GetFileName = Mid(fp, InStrRev(fp, "\") + 1)
End Function
