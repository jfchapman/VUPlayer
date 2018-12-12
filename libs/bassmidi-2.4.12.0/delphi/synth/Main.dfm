object MainForm: TMainForm
  Left = 0
  Top = 0
  BorderStyle = bsDialog
  Caption = 'BASSMIDI Synth'
  ClientHeight = 359
  ClientWidth = 292
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  KeyPreview = True
  OldCreateOrder = False
  Position = poDesktopCenter
  OnCreate = FormCreate
  OnDestroy = FormDestroy
  OnKeyDown = FormKeyDown
  OnKeyUp = FormKeyUp
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object FXGroup: TGroupBox
    Left = 0
    Top = 265
    Width = 292
    Height = 94
    Align = alClient
    Caption = ' FX '
    TabOrder = 0
    object CheckBox1: TCheckBox
      Left = 16
      Top = 15
      Width = 97
      Height = 17
      Caption = 'Chorus'
      TabOrder = 0
      OnClick = FxCheckClick
    end
    object CheckBox3: TCheckBox
      Tag = 2
      Left = 197
      Top = 15
      Width = 87
      Height = 17
      Caption = 'Distortion'
      TabOrder = 1
      OnClick = FxCheckClick
    end
    object CheckBox4: TCheckBox
      Tag = 3
      Left = 16
      Top = 38
      Width = 97
      Height = 17
      Caption = 'Echo'
      TabOrder = 2
      OnClick = FxCheckClick
    end
    object CheckBox5: TCheckBox
      Tag = 4
      Left = 94
      Top = 38
      Width = 97
      Height = 17
      Caption = 'Flanger'
      TabOrder = 3
      OnClick = FxCheckClick
    end
    object CheckBox6: TCheckBox
      Tag = 5
      Left = 197
      Top = 38
      Width = 87
      Height = 17
      Caption = 'Gargle'
      TabOrder = 4
      OnClick = FxCheckClick
    end
    object CheckBox7: TCheckBox
      Tag = 6
      Left = 197
      Top = 61
      Width = 87
      Height = 17
      Caption = 'I3DL2Reverb'
      TabOrder = 5
      OnClick = FxCheckClick
    end
    object CheckBox8: TCheckBox
      Tag = 7
      Left = 16
      Top = 61
      Width = 97
      Height = 17
      Caption = 'Param EQ'
      TabOrder = 6
      OnClick = FxCheckClick
    end
    object CheckBox9: TCheckBox
      Tag = 8
      Left = 94
      Top = 61
      Width = 97
      Height = 17
      Caption = 'Reverb'
      TabOrder = 7
      OnClick = FxCheckClick
    end
    object CheckBox2: TCheckBox
      Tag = 1
      Left = 94
      Top = 15
      Width = 97
      Height = 17
      Caption = 'Compressor'
      TabOrder = 8
      OnClick = FxCheckClick
    end
  end
  object MidiInGroup: TGroupBox
    Left = 0
    Top = 0
    Width = 292
    Height = 57
    Align = alTop
    Caption = ' MIDI input device '
    TabOrder = 1
    object ActivityLabel: TLabel
      Left = 232
      Top = 20
      Width = 52
      Height = 21
      Alignment = taCenter
      AutoSize = False
      Caption = 'Activity'
      Layout = tlCenter
    end
    object MidiCombo: TComboBox
      Left = 8
      Top = 20
      Width = 217
      Height = 21
      Style = csDropDownList
      TabOrder = 0
      OnChange = MidiComboChange
      OnKeyPress = MidiComboKeyPress
    end
  end
  object KeyboardGroup: TGroupBox
    Left = 0
    Top = 57
    Width = 292
    Height = 64
    Align = alTop
    Caption = ' Keyboard input '
    TabOrder = 2
    object InfoLabel: TLabel
      Left = 2
      Top = 15
      Width = 288
      Height = 47
      Align = alClient
      Alignment = taCenter
      AutoSize = False
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -15
      Font.Name = 'Tahoma'
      Font.Style = [fsBold]
      ParentFont = False
      ShowAccelChar = False
      Layout = tlCenter
      ExplicitWidth = 481
      ExplicitHeight = 42
    end
  end
  object SoundFontGroup: TGroupBox
    Left = 0
    Top = 121
    Width = 292
    Height = 88
    Align = alTop
    Caption = ' Soundfont '
    TabOrder = 3
    DesignSize = (
      292
      88)
    object PresetLabel: TLabel
      Left = 8
      Top = 56
      Width = 50
      Height = 17
      AutoSize = False
      Caption = 'Preset:'
      Layout = tlCenter
    end
    object Sf2Button: TButton
      Left = 8
      Top = 17
      Width = 276
      Height = 25
      Anchors = [akLeft, akTop, akRight]
      Caption = 'No Soundfont'
      TabOrder = 0
      OnClick = Sf2ButtonClick
    end
    object PresetCombo: TComboBox
      Left = 48
      Top = 56
      Width = 177
      Height = 21
      AutoComplete = False
      Style = csDropDownList
      TabOrder = 1
      OnChange = PresetComboChange
      OnKeyPress = PresetComboKeyPress
    end
    object Drumcheck: TCheckBox
      Left = 231
      Top = 58
      Width = 58
      Height = 17
      Caption = 'Drums'
      TabOrder = 2
      OnClick = DrumcheckClick
    end
  end
  object BufferGroup: TGroupBox
    Left = 0
    Top = 209
    Width = 292
    Height = 56
    Align = alTop
    Caption = ' Buffer '
    TabOrder = 4
    object BufLabel: TLabel
      Left = 231
      Top = 20
      Width = 58
      Height = 13
      AutoSize = False
      Caption = 'ms'
      Layout = tlCenter
    end
    object BufferTrack: TTrackBar
      Left = 3
      Top = 16
      Width = 222
      Height = 33
      Max = 100
      Position = 50
      ShowSelRange = False
      TabOrder = 0
      TickStyle = tsNone
      OnChange = BufferTrackChange
    end
  end
  object Sf2OpenDialog: TOpenDialog
    Filter = 'Soundfont Files (*.sf2)|*.sf2'
    Left = 16
    Top = 72
  end
  object ActivityTimer: TTimer
    Enabled = False
    Interval = 30
    OnTimer = ActivityTimerTimer
    Left = 56
    Top = 72
  end
end
