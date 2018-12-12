VERSION 5.00
Object = "{F9043C88-F6F2-101A-A3C9-08002B2F49FB}#1.2#0"; "COMDLG32.OCX"
Object = "{831FDD16-0C5C-11D2-A9FC-0000F8754DA1}#2.1#0"; "MSCOMCTL.OCX"
Begin VB.Form frmSynth 
   BorderStyle     =   1  'Fixed Single
   Caption         =   "BASSMIDI synth"
   ClientHeight    =   5625
   ClientLeft      =   45
   ClientTop       =   330
   ClientWidth     =   4335
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MinButton       =   0   'False
   ScaleHeight     =   5625
   ScaleWidth      =   4335
   StartUpPosition =   2  'CenterScreen
   Begin MSComDlg.CommonDialog CMD 
      Left            =   3720
      Top             =   2640
      _ExtentX        =   847
      _ExtentY        =   847
      _Version        =   393216
   End
   Begin VB.Timer tmrSynth 
      Enabled         =   0   'False
      Interval        =   100
      Left            =   3720
      Top             =   120
   End
   Begin VB.Frame frmOutput 
      Caption         =   " Output "
      Height          =   1695
      Left            =   120
      TabIndex        =   12
      Top             =   3840
      Width           =   4095
      Begin VB.CheckBox chkEffects 
         Caption         =   "chorus"
         Height          =   195
         Index           =   2
         Left            =   3000
         TabIndex        =   19
         Top             =   1080
         Width           =   855
      End
      Begin VB.CheckBox chkEffects 
         Caption         =   "distortion"
         Height          =   195
         Index           =   4
         Left            =   1800
         TabIndex        =   18
         Top             =   1320
         Width           =   975
      End
      Begin VB.CheckBox chkEffects 
         Caption         =   "echo"
         Height          =   195
         Index           =   1
         Left            =   1800
         TabIndex        =   17
         Top             =   1080
         Width           =   735
      End
      Begin VB.CheckBox chkEffects 
         Caption         =   "flanger"
         Height          =   195
         Index           =   3
         Left            =   720
         TabIndex        =   16
         Top             =   1320
         Width           =   855
      End
      Begin VB.CheckBox chkEffects 
         Caption         =   "reverb"
         Height          =   195
         Index           =   0
         Left            =   720
         TabIndex        =   15
         Top             =   1080
         Width           =   855
      End
      Begin VB.CheckBox chkSyncInterpolation 
         Caption         =   "Sync interpolation"
         Height          =   255
         Left            =   120
         TabIndex        =   13
         Top             =   720
         Width           =   1575
      End
      Begin MSComctlLib.Slider slideBufferLength 
         Height          =   315
         Left            =   600
         TabIndex        =   21
         Top             =   320
         Width           =   2535
         _ExtentX        =   4471
         _ExtentY        =   556
         _Version        =   393216
         Max             =   50
         SelStart        =   1
         TickStyle       =   3
         TickFrequency   =   0
         Value           =   1
      End
      Begin VB.Label lblMS 
         AutoSize        =   -1  'True
         Caption         =   "51ms"
         Height          =   195
         Left            =   3240
         TabIndex        =   22
         Top             =   360
         Width           =   375
      End
      Begin VB.Label lblBuffer 
         AutoSize        =   -1  'True
         Caption         =   "Buffer:"
         Height          =   195
         Left            =   120
         TabIndex        =   20
         Top             =   360
         Width           =   465
      End
      Begin VB.Label lblEffects 
         AutoSize        =   -1  'True
         Caption         =   "Effects:"
         Height          =   195
         Left            =   120
         TabIndex        =   14
         Top             =   1080
         Width           =   540
      End
   End
   Begin VB.Frame frmSoundfont 
      Caption         =   " Soundfont "
      Height          =   1215
      Left            =   120
      TabIndex        =   2
      Top             =   2520
      Width           =   4095
      Begin VB.CheckBox chkDrums 
         Caption         =   "Drums"
         Height          =   375
         Left            =   3120
         TabIndex        =   10
         Top             =   720
         Width           =   855
      End
      Begin VB.ComboBox cmbPreset 
         Height          =   315
         Left            =   720
         Style           =   2  'Dropdown List
         TabIndex        =   9
         Top             =   720
         Width           =   2175
      End
      Begin VB.CommandButton cmdSoundfont 
         Caption         =   "no soundfont"
         Height          =   375
         Left            =   120
         TabIndex        =   3
         Top             =   240
         Width           =   3855
      End
      Begin VB.Label lblPreset 
         AutoSize        =   -1  'True
         Caption         =   "Preset:"
         Height          =   195
         Left            =   120
         TabIndex        =   11
         Top             =   840
         Width           =   495
      End
   End
   Begin VB.Frame frmKeyboard 
      Caption         =   " Keyboard input "
      Height          =   975
      Left            =   120
      TabIndex        =   1
      Top             =   1440
      Width           =   4095
      Begin VB.Label lblKeys 
         Alignment       =   2  'Center
         BeginProperty Font 
            Name            =   "Courier New"
            Size            =   14.25
            Charset         =   177
            Weight          =   700
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         Height          =   615
         Left            =   120
         TabIndex        =   8
         Top             =   240
         Width           =   3855
      End
   End
   Begin VB.Frame frmMidiInput 
      Caption         =   " MIDI input device "
      Height          =   1215
      Left            =   120
      TabIndex        =   0
      Top             =   120
      Width           =   4095
      Begin VB.OptionButton optChannels 
         Caption         =   "1 channel"
         Height          =   255
         Index           =   0
         Left            =   120
         TabIndex        =   7
         Top             =   840
         Width           =   1095
      End
      Begin VB.OptionButton optChannels 
         Caption         =   "16 channels"
         Height          =   255
         Index           =   1
         Left            =   1320
         TabIndex        =   6
         Top             =   840
         Width           =   1215
      End
      Begin VB.ComboBox cmbMidiDevices 
         Height          =   315
         Left            =   120
         Style           =   2  'Dropdown List
         TabIndex        =   5
         Top             =   360
         Width           =   2895
      End
      Begin VB.CommandButton cmdReset 
         Caption         =   "Reset"
         Height          =   375
         Left            =   2760
         TabIndex        =   4
         Top             =   720
         Width           =   1215
      End
      Begin VB.Label lblActivity 
         Alignment       =   2  'Center
         BackColor       =   &H00FFFFFF&
         BorderStyle     =   1  'Fixed Single
         Caption         =   "activity"
         Height          =   285
         Left            =   3180
         TabIndex        =   23
         Top             =   360
         Width           =   705
      End
   End
End
Attribute VB_Name = "frmSynth"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
'/////////////////////////////////////////////////////////////////////////
' frmSynth.frm - Copyright (c) 2016 (: JOBnik! :) [Arthur Aminov, ISRAEL]
'                                                 [http://www.jobnik.org]
'                                                 [  jobnik@jobnik.org  ]
'
' Other source: modSynth.bas
'
' BASS Simple Synth
' Originally translated from - synth.c - Example of Ian Luck
'/////////////////////////////////////////////////////////////////////////

Option Explicit

Dim formLoaded As Boolean

Private Sub Form_Load()
    ' change and set the current path, to prevent from VB not finding BASS.DLL
    ChDrive App.Path
    ChDir App.Path

    fxtype = Array(BASS_FX_DX8_REVERB, BASS_FX_DX8_ECHO, BASS_FX_DX8_CHORUS, BASS_FX_DX8_FLANGER, BASS_FX_DX8_DISTORTION)
    keys = Array("Q", "2", "W", "3", "E", "R", "5", "T", "6", "Y", "7", "U", "I", "9", "O", "0", "P", 219, 187, 221)

    ' check the correct BASS was loaded
    If (HiWord(BASS_GetVersion()) <> BASSVERSION) Then
        Call Error_("An incorrect version of BASS.DLL was loaded")
        End
    End If

    Call BASS_SetConfig(BASS_CONFIG_VISTA_TRUEPOS, 0) ' allows lower latency on Vista and newer
    Call BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 10) ' 10ms update period
    
    ' initialize default output device (and measure latency)
    If (BASS_Init(-1, 44100, BASS_DEVICE_LATENCY, 0, 0) = 0) Then
        Call Error_("Can't initialize output device")
        End
    End If

    Call BASS_GetInfo(info)
    Call BASS_SetConfig(BASS_CONFIG_BUFFER, 10 + info.minbuf + 1) ' default buffer size = update period + 'minbuf' + 1ms margin
    stream = BASS_MIDI_StreamCreate(17, 0, 1) ' create the MIDI stream (16 MIDI channels for device input + 1 for keyboard input)
    Call BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_CPU, 75) ' limit CPU usage to 75% (also enables async sample loading)
    Call BASS_ChannelSetSync(stream, BASS_SYNC_MIDI_EVENT Or BASS_SYNC_MIXTIME, MIDI_EVENT_PROGRAM, AddressOf ProgramEventSync, 0) ' catch program/preset changes
    Call BASS_MIDI_StreamEvent(stream, 0, MIDI_EVENT_SYSTEM, MIDI_SYSTEM_GS) ' send GS system reset event
    Call BASS_ChannelPlay(stream, 0) ' start it

    Me.KeyPreview = True

    preset = 0
    drums = 0
    chans16 = 0
    activity = 0

    lblKeys.Caption = "  2 3  5 6 7  9 0  =" & vbCrLf & " Q W ER T Y UI O P[ ]"

    lblMS.Caption = BASS_GetConfig(BASS_CONFIG_BUFFER) & "ms"

    ' enumerate available input devices
    Dim di As BASS_MIDI_DEVICEINFO
    Dim dev As Integer
    
    dev = 0
    While BASS_MIDI_InGetDeviceInfo(dev, di)
        cmbMidiDevices.AddItem VBStrFromAnsiPtr(di.name)
        dev = dev + 1
    Wend
    If (dev) Then ' got sone, try to initialize one
        Dim a As Integer
        For a = 0 To dev - 1
            If (BASS_MIDI_InInit(a, AddressOf MidiInProcCallback, 0)) Then  ' succeeded, start it
                input_ = a
                Call BASS_MIDI_InStart(input_)
                cmbMidiDevices.ListIndex = input_
                Exit For
            End If
        Next a
        If (a = dev) Then Error_ ("Can't initialize MIDI device")
    Else
        cmbMidiDevices.AddItem "no devices"
        cmbMidiDevices.ListIndex = 0
        cmbMidiDevices.Enabled = False
        optChannels(0).Enabled = False
        optChannels(1).Enabled = False
    End If
    optChannels(0).value = 1

    ' get default font (28mbgm.sf2/ct8mgm.sf2/ct4mgm.sf2/ct2mgm.sf2 if available)
    Dim sf As BASS_MIDI_FONT
    Dim i As BASS_MIDI_FONTINFO
    If (BASS_MIDI_StreamGetFonts(0, sf, 1)) Then
        font_ = sf.font
        Call BASS_MIDI_FontGetInfo(font_, i)
        cmdSoundfont.Caption = VBStrFromAnsiPtr(i.name)
    End If

    Call UpdatePresetList

    activityColors(0) = "&HFFFFFF"
    activityColors(1) = "&H00FF00"

    ' load optional plugins for packed soundfonts (others may be used too)
    Call BASS_PluginLoad("bassflac.dll", 0)
    Call BASS_PluginLoad("basswv.dll", 0)

    formLoaded = True
End Sub

Private Sub chkDrums_Click()
    drums = chkDrums.value
    Call BASS_MIDI_StreamEvent(stream, 16, MIDI_EVENT_DRUMS, drums) ' send drum switch event
    preset = BASS_MIDI_StreamGetEvent(stream, 16, MIDI_EVENT_PROGRAM) ' preset is reset in drum switch
    Call UpdatePresetList
    Call BASS_MIDI_FontCompact(0) ' unload unused samples
End Sub

Private Sub chkEffects_Click(index As Integer)
    If (fx(index)) Then
        Call BASS_ChannelRemoveFX(stream, fx(index))
        fx(index) = 0
    Else
        fx(index) = BASS_ChannelSetFX(stream, fxtype(index), index)
    End If
End Sub

Private Sub chkSyncInterpolation_Click()
    If (chkSyncInterpolation.value = 1) Then
        Call BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_SRC, 1); ' enable sinc interpolation
    Else
        Call BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_SRC, 0); ' disable sinc interpolation
    End If
End Sub

Private Sub cmbMidiDevices_Click()
    If (formLoaded) Then
        Call BASS_MIDI_InFree(input_) ' free current input device
        input_ = cmbMidiDevices.ListIndex ' get new input device selection
        If (BASS_MIDI_InInit(input_, AddressOf MidiInProcCallback, 0)) Then  ' successfully initialized...
            Call BASS_MIDI_InStart(input_) ' start it
        Else
            Call Error_("Can't initialize MIDI device")
        End If
    End If
End Sub

Private Sub cmbPreset_Click()
    If (formLoaded) Then
        preset = cmbPreset.ListIndex
        Call BASS_MIDI_StreamEvent(stream, 16, MIDI_EVENT_PROGRAM, preset) 'send program/preset event
        Call BASS_MIDI_FontCompact(0) ' unload unused samples
    End If
End Sub

Private Sub cmdReset_Click()
    Call BASS_MIDI_StreamEvent(stream, 0, MIDI_EVENT_SYSTEM, MIDI_SYSTEM_GS) ' send system reset event
    If (drums) Then Call BASS_MIDI_StreamEvent(stream, 16, MIDI_EVENT_DRUMS, drums) ' send drum switch event
    Call BASS_MIDI_StreamEvent(stream, 16, MIDI_EVENT_PROGRAM, preset) ' send program/preset event
End Sub

Private Sub cmdSoundfont_Click()
    On Local Error Resume Next    ' in case Cancel was pressed

    With CMD
        .CancelError = True
        .flags = cdlOFNExplorer Or cdlOFNFileMustExist
        .filter = "Soundfonts (sf2/sf2pack/sfz)|*.sf2;*.sf2pack;*.sfz|All files|*.*"
        .ShowOpen
    End With

    ' if cancel was pressed then exit
    If Err.Number = 32755 Then Exit Sub

    Dim newfont As Long
    newfont = BASS_MIDI_FontInit(CMD.filename, 0)
    If (newfont) Then
        Dim sf As BASS_MIDI_FONT
        sf.font = newfont
        sf.preset = -1 ' use all presets
        sf.bank = 0 ' use default bank(s)
        Call BASS_MIDI_StreamSetFonts(0, sf, 1) ' set default soundfont
        Call BASS_MIDI_StreamSetFonts(stream, sf, 1) ' apply to current stream too
        Call BASS_MIDI_FontFree(font_) ' free old soundfont
        font_ = newfont
        
        Dim i As BASS_MIDI_FONTINFO
        Call BASS_MIDI_FontGetInfo(font_, i)
        cmdSoundfont.Caption = IIf(i.name, VBStrFromAnsiPtr(i.name), GetFileName(CMD.filename))
        If (i.presets = 1) Then ' only 1 preset, auto-select it...
            Dim p As Long
            Call BASS_MIDI_FontGetPresets(font_, p)
            drums = IIf(HiWord(p), 128, 0) ' bank 128 = drums
            preset = LoWord(p)
            chkDrums.value = drums
            Call BASS_MIDI_StreamEvent(stream, 16, MIDI_EVENT_DRUMS, drums) ' send drum switch event
            Call BASS_MIDI_StreamEvent(stream, 16, MIDI_EVENT_PROGRAM, preset) ' send program/preset event
            cmbPreset.Enabled = False
            chkDrums.Enabled = False
        Else
            cmbPreset.Enabled = True
            chkDrums.Enabled = True
        End If

        Call UpdatePresetList
    End If
End Sub

Private Sub optChannels_Click(index As Integer)
    chans16 = optChannels(1).value ' MIDI input channels
End Sub

Private Sub slideBufferLength_Scroll()
    Dim p As Integer
   
    p = slideBufferLength.value
    Call BASS_SetConfig(BASS_CONFIG_BUFFER, 10 + info.minbuf + p) ' update buffer length

    ' recreate the MIDI stream with new buffer length
    Call BASS_StreamFree(stream)
    stream = BASS_MIDI_StreamCreate(17, 0, 1)
    If (chkSyncInterpolation.value = 1) Then Call BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_SRC, 1); ' enable sinc interpolation
    Call BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_CPU, 75)
    Call BASS_ChannelSetSync(stream, BASS_SYNC_MIDI_EVENT Or BASS_SYNC_MIXTIME, MIDI_EVENT_PROGRAM, AddressOf ProgramEventSync, 0)
    Call BASS_MIDI_StreamEvent(stream, 0, MIDI_EVENT_SYSTEM, MIDI_SYSTEM_GS) ' send GS system reset event

    If (drums) Then Call BASS_MIDI_StreamEvent(stream, 16, MIDI_EVENT_DRUMS, drums) ' set drum switch
    Call BASS_MIDI_StreamEvent(stream, 16, MIDI_EVENT_PROGRAM, preset) ' set program/preset

    ' re-apply effects
    Dim a As Integer
    For a = 0 To 4
        If (fx(a)) Then fx(a) = BASS_ChannelSetFX(stream, fxtype(a), a)
    Next a

    Call BASS_ChannelPlay(stream, 0)
    
    lblMS.Caption = BASS_GetConfig(BASS_CONFIG_BUFFER) & "ms"
End Sub

Private Sub tmrSynth_Timer()
    tmrSynth.Enabled = False
    activity = 0
    lblActivity.BackColor = activityColors(0)
End Sub

Private Sub Form_KeyDown(KeyCode As Integer, Shift As Integer)
    Call keyDU(KeyCode, False)
End Sub

Private Sub Form_KeyUp(KeyCode As Integer, Shift As Integer)
    Call keyDU(KeyCode, True)
End Sub

Private Sub Form_Unload(Cancel As Integer)
    ' release everything
    Call BASS_MIDI_InFree(input_)
    Call BASS_Free
    Call BASS_PluginFree(0)
End Sub

