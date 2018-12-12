object Form1: TForm1
  Left = 268
  Top = 78
  BorderStyle = bsDialog
  Caption = 'BASSMIDI test'
  ClientHeight = 216
  ClientWidth = 375
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnClose = FormClose
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object lbTitle: TLabel
    Left = 8
    Top = 42
    Width = 3
    Height = 13
  end
  object btnOpen: TButton
    Left = 8
    Top = 8
    Width = 360
    Height = 23
    Caption = 'click here to open a file...'
    TabOrder = 0
    OnClick = btnOpenClick
  end
  object TrackBar: TTrackBar
    Left = 6
    Top = 61
    Width = 255
    Height = 24
    Orientation = trHorizontal
    Frequency = 1
    Position = 0
    SelEnd = 0
    SelStart = 0
    TabOrder = 1
    TickMarks = tmBoth
    TickStyle = tsNone
    OnChange = TrackBarChange
  end
  object gbSoundfonf: TGroupBox
    Left = 6
    Top = 160
    Width = 362
    Height = 52
    Caption = ' Soundfont '
    TabOrder = 3
    object lbSoundfont: TLabel
      Left = 8
      Top = 16
      Width = 62
      Height = 13
      Caption = 'no soundfont'
    end
    object btnReplace: TButton
      Left = 280
      Top = 16
      Width = 75
      Height = 25
      Caption = 'Replace'
      TabOrder = 0
      OnClick = btnReplaceClick
    end
  end
  object gbLyrics: TGroupBox
    Left = 6
    Top = 97
    Width = 362
    Height = 63
    Caption = ' Lyrics '
    TabOrder = 2
    object lbLyrics: TLabel
      Left = 2
      Top = 13
      Width = 356
      Height = 39
      Alignment = taCenter
      AutoSize = False
    end
  end
  object cbEffects: TCheckBox
    Left = 262
    Top = 58
    Width = 101
    Height = 29
    Caption = 'Reverb && Chorus'
    Checked = True
    State = cbChecked
    TabOrder = 4
    OnClick = cbEffectsClick
  end
  object Timer1: TTimer
    Interval = 500
    OnTimer = Timer1Timer
    Left = 336
    Top = 32
  end
end
