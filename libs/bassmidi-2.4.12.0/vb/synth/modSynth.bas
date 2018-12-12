Attribute VB_Name = "modSynth"
'/////////////////////////////////////////////////////////////////////////
' modSynth.bas - Copyright (c) 2016 (: JOBnik! :) [Arthur Aminov, ISRAEL]
'                                                 [http://www.jobnik.org]
'                                                 [  jobnik@jobnik.org  ]
'
' Other source: frmSynth.frm
'
' BASS Simple Synth
' Originally translated from - synth.c - Example of Ian Luck
'/////////////////////////////////////////////////////////////////////////

Option Explicit

Public info As BASS_INFO

Public Const KEYS_ = 20
Public keys As Variant

Public fx(5) As Long        ' effect handles
Public fxtype As Variant

Public input_ As Long       ' MIDI input device
Public stream As Long       ' output stream
Public font_ As Long        ' soundfont
Public preset As Long       ' current preset
Public drums As Long        ' drums enabled?
Public chans16 As Long      ' 16 MIDI channels?
Public activity As Long
Public activityColors(2) As String

Public Const WM_CHAR = &H102

Public Type POINTAPI
    X As Long
    Y As Long
End Type

Public Type MSG
    hwnd As Long
    message As Long
    wParam As Long
    lparam As Long
    time As Long
    pt As POINTAPI
End Type

Public Declare Function PeekMessage Lib "user32" Alias "PeekMessageA" (lpMsg As MSG, ByVal hwnd As Long, ByVal wMsgFilterMin As Long, ByVal wMsgFilterMax As Long, ByVal wRemoveMsg As Long) As Long

' display error messages
Public Sub Error_(ByVal es As String)
    Call MsgBox(es & vbCrLf & vbCrLf & "error code: " & BASS_ErrorGetCode, vbExclamation, "Error")
End Sub

' MIDI input function
Public Sub MidiInProcCallback(ByVal handle As Long, ByVal time As Double, ByVal buffer As Long, ByVal length As Long, ByVal user As Long)
    If (chans16) Then ' using 16 channels
        Call BASS_MIDI_StreamEvents(stream, BASS_MIDI_EVENTS_RAW, buffer, length) ' send MIDI data to the MIDI stream
    Else
        Call BASS_MIDI_StreamEvents(stream, (BASS_MIDI_EVENTS_RAW + 17) Or BASS_MIDI_EVENTS_SYNC, buffer, length) ' send MIDI data to channel 17 in the MIDI stream
    End If
    activity = 1
    frmSynth.lblActivity.BackColor = activityColors(1)
    frmSynth.tmrSynth.Enabled = True
End Sub

' program/preset event sync function
Public Sub ProgramEventSync(ByVal handle As Long, ByVal channel As Long, ByVal data As Long, ByVal user As Long)
    preset = LoWord(data)
    frmSynth.cmbPreset.ListIndex = preset  ' update the preset selector
    Call BASS_MIDI_FontCompact(0) ' unload unused samples
End Sub

Public Sub UpdatePresetList()
    Dim a As Integer
    With frmSynth
        .cmbPreset.Clear

        Dim text As String
        Dim name As String

        For a = 0 To 127
            name = VBStrFromAnsiPtr(BASS_MIDI_FontGetPreset(font_, a, IIf(drums, 128, 0))) ' get preset name
            text = Format(a, "000") & ": " & IIf(name <> "", name, "")
            .cmbPreset.AddItem text
        Next a
        .cmbPreset.ListIndex = 0
    End With
End Sub

Public Sub keyDU(ByVal KeyCode As Integer, ByVal isKeyUp As Boolean)
    Dim lpMsg As MSG

    If (isKeyUp Or PeekMessage(lpMsg, 0, 0, 0, 0) > 0) Then
        If (isKeyUp Or (lpMsg.message = WM_CHAR And (lpMsg.lparam And &H40000000) = 0)) Then  ' prevent key flooding
            Dim key As Long
            For key = 0 To KEYS_ - 1
                If (KeyCode = keys(key) Or KeyCode = Asc(keys(key))) Then
                    Call BASS_MIDI_StreamEvent(stream, 16, MIDI_EVENT_NOTE, MakeWord((IIf(drums, 36, 60)) + key, IIf(isKeyUp, 0, 100)))  ' send note on/off event
                    Exit For
                End If
            Next key
        End If
    End If
End Sub

' get file name from file path
Public Function GetFileName(ByVal fp As String) As String
    GetFileName = Mid(fp, InStrRev(fp, "\") + 1)
End Function
