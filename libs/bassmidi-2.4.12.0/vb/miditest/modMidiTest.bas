Attribute VB_Name = "modMidiTest"
'=================================================================================
' modMidiTest.bas - Copyright (c) 2006-2010 (: JOBnik! :) [Arthur Aminov, ISRAEL]
'                                                         [http://www.jobnik.org]
'                                                         [  jobnik@jobnik.org  ]
' Other sources: frmMidiTest.frm
'
' BASSMIDI test
' * Requires: BASS 2.4 (available @ www.un4seen.com)
'=================================================================================

Option Explicit

Public chan As Long            ' channel handle
Public font_ As Long           ' soundfont

Public miditempo As Long       ' MIDI file tempo
Public temposcale As Single    ' tempo adjustment

Public lyrics As String        ' lyrics buffer

Private Declare Sub CopyMemory Lib "kernel32" Alias "RtlMoveMemory" (ByRef Destination As Any, ByRef Source As Any, ByVal length As Long)


Public Sub LyricSync(ByVal handle As Long, ByVal channel As Long, ByVal data As Long, ByVal user As Long)
    Dim mark As BASS_MIDI_MARK
    Dim text As String

    Call BASS_MIDI_StreamGetMark(channel, user, data, mark)  ' get the lyric/text

    text = VBStrFromAnsiPtr(mark.text)
    If (Left(text, 1) = "@") Then Exit Sub ' skip info

    Dim lines As Long, tmp As Integer

    If (Left(text, 1) = "\") Then ' clear display
        lyrics = ""
        text = Right(text, Len(text) - 1)
    ElseIf (Left(text, 1) = "/") Then ' new line
        lyrics = lyrics & vbCrLf
        text = Right(text, Len(text) - 1)
    End If
    
    ' count lines
    lines = 1
    For tmp = 1 To Len(lyrics)
        tmp = InStr(tmp, lyrics, vbCrLf)
        If (tmp = 0) Then Exit For
        lines = lines + 1
    Next tmp

    ' remove old lines so that new lines fit in display
    If (lines > 3) Then lyrics = ""

    lyrics = lyrics & text
    frmMidiTest.lblLyrics.Caption = lyrics
End Sub

Public Sub EndSync(ByVal handle As Long, ByVal channel As Long, ByVal data As Long, ByVal user As Long)
    lyrics = "" ' clear lyrics
    frmMidiTest.lblLyrics.Caption = ""
End Sub

Public Sub SetTempo(ByVal reset As Boolean)
    If (reset) Then miditempo = BASS_MIDI_StreamGetEvent(chan, 0, MIDI_EVENT_TEMPO) ' get the file's tempo
    Call BASS_MIDI_StreamEvent(chan, 0, MIDI_EVENT_TEMPO, miditempo * temposcale) ' set tempo
End Sub

Public Sub TempoSync(ByVal handle As Long, ByVal channel As Long, ByVal data As Long, ByVal user As Long)
    Call SetTempo(True) ' override the tempo
End Sub

' look for a marker (eg. loop points)
Public Function FindMarker(ByVal handle As Long, ByVal text As String, ByRef mark As BASS_MIDI_MARK) As Boolean
    Dim a As Long

    Do While BASS_MIDI_StreamGetMark(Handle, BASS_MIDI_MARK_MARKER, a, mark)
        If (VBStrFromAnsiPtr(mark.Text) = text) Then  ' found it
            FindMarker = True
            Exit Function
        End If
        a = (a + 1)
    Loop
End Function

Public Sub LoopSync(ByVal handle As Long, ByVal channel As Long, ByVal data As Long, ByVal user As Long)
    Dim mark As BASS_MIDI_MARK

    If (FindMarker(channel, "loopstart", mark)) Then ' found a loop start point
        Call BASS_ChannelSetPosition(channel, mark.pos, BASS_POS_BYTE Or BASS_MIDI_DECAYSEEK) ' rewind to it (and let old notes decay)
    Else
        Call BASS_ChannelSetPosition(channel, 0, BASS_POS_BYTE Or BASS_MIDI_DECAYSEEK) ' else rewind to the beginning instead
    End If
End Sub
