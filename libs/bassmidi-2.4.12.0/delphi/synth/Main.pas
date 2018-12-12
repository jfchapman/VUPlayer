unit Main;

(*---------------------------------------------------------------------------
  BASSMIDI Synth example for Delphi using BASS.DLL and BASSMidi.dll.

  Adaptation from the C Synth example by Un4Seen Developments (2011)
  by André Lubke 2011.
  ---------------------------------------------------------------------------

  This example is programmed in Delphi XE and not tested on other versions
  of Delphi.

  Some issues can occur if you try to compile this project with an earlier
  version of Delphi:
  - If you get a warning about unrecognized properties when loading this project:
      Push the ignore button and you should be fine.

  --------------------------------------------------------------------------- *)

interface

uses
  Windows, Messages, SysUtils, Variants, Classes, Graphics, Controls, Forms,
  Dialogs, Bass, BassMidi, StdCtrls, Buttons, ExtCtrls, ComCtrls;

type
  TMainForm = class(TForm)
    FXGroup: TGroupBox;
    CheckBox1: TCheckBox;
    CheckBox3: TCheckBox;
    CheckBox4: TCheckBox;
    CheckBox5: TCheckBox;
    CheckBox6: TCheckBox;
    CheckBox7: TCheckBox;
    CheckBox8: TCheckBox;
    CheckBox9: TCheckBox;
    MidiInGroup: TGroupBox;
    MidiCombo: TComboBox;
    KeyboardGroup: TGroupBox;
    InfoLabel: TLabel;
    SoundFontGroup: TGroupBox;
    Sf2Button: TButton;
    PresetCombo: TComboBox;
    Sf2OpenDialog: TOpenDialog;
    PresetLabel: TLabel;
    Drumcheck: TCheckBox;
    BufferGroup: TGroupBox;
    BufferTrack: TTrackBar;
    CheckBox2: TCheckBox;
    BufLabel: TLabel;
    ActivityLabel: TLabel;
    ActivityTimer: TTimer;
    procedure FormCreate(Sender: TObject);
    procedure FormShow(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure DrumcheckClick(Sender: TObject);
    procedure FxCheckClick(Sender: TObject);
    procedure FormKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
    procedure FormKeyUp(Sender: TObject; var Key: Word; Shift: TShiftState);
    procedure Sf2ButtonClick(Sender: TObject);
    procedure MidiComboChange(Sender: TObject);
    procedure SetMidiInDevice;
    procedure LoadSoundFont;
    procedure UpdatePresetList;
    procedure PresetComboChange(Sender: TObject);
    procedure BufferTrackChange(Sender: TObject);
    procedure ActivityTimerTimer(Sender: TObject);
    procedure PresetComboKeyPress(Sender: TObject; var Key: Char);
    procedure MidiComboKeyPress(Sender: TObject; var Key: Char);
  private
    { Private declarations }
  public
    { Public declarations }
    procedure FatalError(Msg : string);
    procedure BASSClose;
  end;


const
  // ---------------- QUERTY Keyboard keys -----------------
  cNoKeys = 20;
  cKeys : array[1..cNoKeys] of Char =
	       ('Q', '2', 'W', '3', 'E', 'R', '5', 'T', '6', 'Y', '7', 'U', 'I', '9',
          'O', '0', 'P', #219, #187, #221);

var
  MainForm      : TMainForm;
  Sf2File       : string;         //Name of the soundfont file to load
  MidiDeviceIdx : integer = -1;   //Index of the midi device to initialize
  MidiStream    : HSTREAM;
  MidiFont      : BASS_MIDI_FONT;
  BassInfo      : BASS_INFO;
  BufLen        : integer;
  Drums         : integer;
  Fx            : array[0..8] of HFX;
  Pressed       : array[1..cNoKeys] of boolean;
  OrigColor     : TColor;
  TimerCount    : integer;

implementation

{$R *.dfm}

// ----------------------- Call Back function for MIDI ------------------------
procedure MIDIINPROC(Handle: DWORD;
                     Time  : Double;
                     Buffer: Pointer;
                     Length: DWORD;
                     User  : Pointer); stdCall;
begin
  // Send MIDI data to output MIDI stream
  BASS_MIDI_StreamEvents (MidiStream, BASS_MIDI_EVENTS_RAW, Buffer, Length);

  // Color the activity label red
  MainForm.ActivityLabel.Color   := clRed;
  MainForm.ActivityTimer.Enabled := true;
end;

// --------- Helper function for translating virtual Delphy Key codes ---------
function GetCharFromVirtualKey(Key: Word): Char;
var
  KeyboardState: TKeyboardState;
  AsciiResult  : Integer;
  TmpStr       : string;
begin
  Result := #0;

  GetKeyboardState(KeyboardState) ;
  SetLength(TmpStr, 2) ;
  AsciiResult := ToAscii(Key, MapVirtualKey(Key, 0), KeyboardState, @TmpStr[1], 0) ;

  case AsciiResult of
    0: TmpStr := '';
    1: SetLength(TmpStr, 1) ;
    2:;
    else
      TmpStr := '';
  end;

  if Length(TmpStr) >= 1 then
    Result := TmpStr[1];
end;

//Set new MIDIin device
procedure TMainForm.SetMidiInDevice;
begin
  if BASS_MIDI_InInit(MidiDeviceIdx, @MidiInProc, nil) then
    BASS_MIDI_InStart(MidiDeviceIdx)
  else
    fatalError('Error initalizing MIDI device.');
end;

//Load a Soundfont from a disk file
procedure TMainForm.LoadSoundFont;
begin
  if (trim(Sf2File) <> '') then
  begin
    BASS_MIDI_FontFree(MidiFont.Font); // free old soundfont
    MidiFont.font := BASS_MIDI_FontInit(PChar(Sf2File), 0 {$IFDEF UNICODE} or BASS_UNICODE {$ENDIF});
    if (MidiFont.font <> 0) then
    begin
		  MidiFont.Preset := -1; //all presets
			MidiFont.Bank   := 0;  //default bank(s)
			BASS_MIDI_StreamSetFonts(0, PBASS_MIDI_FONT(@MidiFont), 1); //make it the default
			BASS_MIDI_StreamSetFonts(MidiStream, PBASS_MIDI_FONT(@MidiFont), 1); //apply to output stream too
		end
    else
      fatalError('Error loading soundfont: '+ extractFileName(sf2File)+ '.');
  end;
  UpdatePresetList;
end;

//Update the preset list if a new soundfont is loaded
procedure TMainForm.UpdatePresetList;
var
  i     : integer;
  Number: string;
  Name  : PAnsiChar;
  Bank  : integer;
begin
  PresetCombo.Clear;
  if Drums = 0 then
    Bank := 0
  else
    Bank := 128;

//Enumerate presets and add them to Combobox
  for i := 0 to 127 do
  begin
    Number := format('%3.3d : ', [i]);
    Name   := BASS_MIDI_FONTGETPRESET(MidiFont.font, i, Bank);
    PresetCombo.Items.Add(Number + Name);
  end;

//Set default preset
  if (PresetCombo.Items.Count > 0) then
  begin
    PresetCombo.ItemIndex := 0;
    PresetComboChange(self);
  end;
end;

//-----------------------------------------------------------------------------
procedure TMainForm.FormCreate(Sender: TObject);
begin
  if (HIWORD(BASS_GetVersion) <> BASSVERSION) then
    fatalError('An incorrect version of BASS.DLL was loaded');
end;

//-----------------------------------------------------------------------------
procedure TMainForm.FormShow(Sender: TObject);
var
  Dev      : DWORD;
  MidiInfo : BASS_MIDI_DEVICEINFO;
begin

//Check for Midi input devices and fill the Midi in combobox
  Dev := 0;
  while BASS_MIDI_InGetDeviceInfo(dev, MidiInfo) do
  begin
    MidiCombo.Items.Add(string(MidiInfo.name));
    inc(dev);
  end;

//Set the default MIDI in device
  if MidiCombo.Items.Count > 0 then
  begin
    MidiCombo.ItemIndex := 0;
    MidiDeviceIdx := MidiCombo.ItemIndex;
    SetMidiInDevice;
  end;

  Drums := 0;
  if BASS_MIDI_StreamGetFonts(0, PBASS_MIDI_FONT(@MidiFont), 1) > 0 then
  begin
    Sf2Button.Caption := 'Default Creative Soundfont';
    UpdatePresetList;
  end;

	BASS_SetConfig(BASS_CONFIG_VISTA_TRUEPOS, 0); // allows lower latency on Vista and newer
	BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 10); // 10ms update period

	// initialize default output device (and measure latency)
	if not BASS_Init(-1, 44100, BASS_DEVICE_LATENCY, 0, nil) then
		FatalError('Can''t initialize device,');

	// load optional plugins for packed soundfonts (others may be used too)
	BASS_PluginLoad('bassflac.dll', 0);
	BASS_PluginLoad('basswv.dll',  0);

	BASS_GetInfo(BASSInfo);
  //Default buffer size = update period + 'minbuf'
	BASS_SetConfig(BASS_CONFIG_BUFFER, 10 + BASSInfo.minbuf);
	BufLen     := BASS_GetConfig(BASS_CONFIG_BUFFER);
  //create and start the output MIDI stream (with 1 MIDI channel)
  MidiStream := BASS_MIDI_StreamCreate(1, 0, 1);
	BASS_ChannelPlay(MidiStream, false);

  //Set default buffer size on trackbar
  BufferTrack.Min := BufLen;
  BufferTrack.Max := 100 + BufLen;
  BufferTrack.Position := BufLen;
  BufferTrackChange(self);

  InfoLabel.Caption :=  '  2  3   5  6  7   9  0'#13#10 +
			                  ' Q  W  ER  T  Y  UI  O  P';

  OrigColor := ActivityLabel.Color;

end;

//---------------------------------------------------------------------------
procedure TMainForm.FormDestroy(Sender: TObject);
begin
  BASSClose; //Make sure BASS and BASSMIDI are freed
end;

//----------------------------- EVENTS --------------------------------------
//Select an effect
procedure TMainForm.FxCheckClick(Sender: TObject);
begin
  with Sender as TCheckBox do
  begin
    if (tag < 0) or (tag > 8) then exit;
    if (checked) then
        fx[tag] := BASS_ChannelSetFX(MidiStream, BASS_FX_DX8_CHORUS + tag, 0)
    else
    begin
        BASS_ChannelRemoveFX(MidiStream, fx[tag]);
        fx[tag] := 0;
    end;
  end;
end;

//Change the MIDI in device
procedure TMainForm.MidiComboChange(Sender: TObject);
begin
  if (MidiDeviceIdx >= 0) then
    BASS_MIDI_InFree(MidiDeviceIdx);

  MidiDeviceIdx := MidiCombo.ItemIndex;
  SetMidiInDevice;
end;

//Select a preset in the soundfont
procedure TMainForm.PresetComboChange(Sender: TObject);
begin
    BASS_MIDI_StreamEvent(MidiStream, 0, MIDI_EVENT_PROGRAM, PresetCombo.ItemIndex);
    BASS_MIDI_FontCompact(0);  // unload unused samples
end;

// Do not respond to keys if focused (otherwise QUERTY tone play does not work)
procedure TMainForm.MidiComboKeyPress(Sender: TObject; var Key: Char);
begin
  Key := #0;
end;

// Do not respond to keys if focused (otherwise QUERTY tone play does not work)
procedure TMainForm.PresetComboKeyPress(Sender: TObject; var Key: Char);
begin
  Key := #0;
end;

//Load a soundfont
procedure TMainForm.Sf2ButtonClick(Sender: TObject);
begin
  if not Sf2OpenDialog.Execute then exit;
  Sf2File := Sf2OpenDialog.FileName;
  Sf2Button.Caption := extractFileName(Sf2OpenDialog.FileName);
  LoadSoundFont;
end;

//Change the buffer size
procedure TMainForm.BufferTrackChange(Sender: TObject);
var
  i : integer;
begin
  bufLen := BufferTrack.Position;
  bufLabel.Caption := inttostr(bufLen) + ' ms';

  //Recreate stream with different buffer
  BASS_SetConfig(BASS_CONFIG_BUFFER, buflen);
  BufLen     := BASS_GetConfig(BASS_CONFIG_BUFFER);
  BASS_StreamFree(MidiStream);
  MidiStream := BASS_MIDI_StreamCreate(1, 0, 1);

  //Set preset/drums/effects on the new stream
  BASS_MIDI_StreamEvent(MidiStream, 0, MIDI_EVENT_DRUMS, Drums);
  BASS_MIDI_StreamEvent(MidiStream, 0, MIDI_EVENT_PROGRAM, PresetCombo.ItemIndex);


  //Apply FX again
  for i := 0 to 8 do
  begin
    if (fx[i] <> 0) then
      fx[i] := BASS_ChannelSetFX(MidiStream, BASS_FX_DX8_CHORUS + i, 0);
  end; {for}

  BASS_ChannelPlay(MidiStream, false);
end;

//Toggle the drums
procedure TMainForm.DrumcheckClick(Sender: TObject);
begin
  if (DrumCheck.Checked) then
    Drums := 1
  else
    Drums := 0;

  BASS_MIDI_StreamEvent(MidiStream, 0, MIDI_EVENT_DRUMS, Drums);
  PresetCombo.ItemIndex := BASS_MIDI_StreamGetEvent(MidiStream,0,MIDI_EVENT_PROGRAM); // preset is reset in drum switch
  UpdatePresetList;
  BASS_MIDI_FontCompact(0); // unload unused samples
end;

//Keys down on QUERTY keyboard
procedure TMainForm.FormKeyDown(Sender: TObject; var Key: Word;
  Shift: TShiftState);
var
  C : char;
  i : integer;
begin
  C := upCase(GetCharFromVirtualKey(key));
		for i := 1 to cNoKeys do
    begin
      if (C = cKeys[i]) then
      begin
        if not pressed[i] then
          pressed[i] := BASS_MIDI_StreamEvent(MidiStream, 0, MIDI_EVENT_NOTE, MAKEWORD(60 - drums * 24 + i, 100))
      end;
    end;
end;

//Keys up on QUERTY keyboard
procedure TMainForm.FormKeyUp(Sender: TObject; var Key: Word;
  Shift: TShiftState);
var
  C : char;
  i : integer;
begin
  C := upCase(GetCharFromVirtualKey(key));
		for i := 1 to cNoKeys do
    begin
      if (C = cKeys[i]) then
      begin
        pressed [i] :=  not BASS_MIDI_StreamEvent(MidiStream, 0, MIDI_EVENT_NOTE, 60 - drums * 24 + i);
      end;
    end;
end;

//Coloring of the 'Activity' label
procedure TMainForm.ActivityTimerTimer(Sender: TObject);
begin
  if TimerCount < 10 then
    inc(TimerCount)
  else
  begin
    ActivityLabel.Color := OrigColor;
    TimerCount := 0;
    ActivityTimer.Enabled := false;
  end;
end;

//Give error and quit
procedure TMainForm.FatalError(Msg: string);
begin
  showMessage('Fatal Error: '+ Msg);
  BASSClose;
  Application.Terminate;
end;

//Free BASS
procedure TMainForm.BASSClose;
begin
  BASS_MIDI_InStop(MidiDeviceIdx);
  BASS_MIDI_Infree(MidiDeviceIdx);
  BASS_FREE;
end;

end.
