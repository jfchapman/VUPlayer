unit Unit1;

interface

uses
  Windows, Messages, SysUtils, Classes, Controls, Forms,
  Dialogs, StdCtrls, ComCtrls, ExtCtrls, Bass, BassMidi;

type
  TForm1 = class(TForm)
    btnOpen     : TButton;
    btnReplace  : TButton;
    cbEffects   : TCheckBox;
    gbLyrics    : TGroupBox;
    gbSoundfonf : TGroupBox;
    lbLyrics    : TLabel;
    lbSoundfont : TLabel;
    lbTitle     : TLabel;
    Timer1      : TTimer;
    TrackBar    : TTrackBar;
    procedure FormCreate      (Sender : TObject);
    procedure FormClose       (Sender : TObject; var Action : TCloseAction);
    procedure cbEffectsClick  (Sender : TObject);
    procedure btnOpenClick    (Sender : TObject);
    procedure btnReplaceClick (Sender : TObject);
    procedure Timer1Timer     (Sender : TObject);
    procedure TrackBarChange  (Sender : TObject);
  private
    { Private declarations }
    fStream     : HSTREAM;
    fSoundfont  : HSOUNDFONT;
    procedure Error (msg : string);

  public
    { Public declarations }
  end;

var
  Form1   : TForm1;

procedure EndSync  (Handle : HSYNC; Stream : HSTREAM; data : DWORD; user : Pointer); stdcall;
procedure LyricSync(Handle : HSYNC; Stream : HSTREAM; data : DWORD; user : Pointer); stdcall;
procedure TextSync (Handle : HSYNC; Stream : HSTREAM; data : DWORD; user : Pointer); stdcall;

implementation

{$R *.dfm}

{--------------------------------------------------------------------------------------------------}

procedure TForm1.Error(msg : string);
var
	s : string;
begin
	s := msg + #13#10 + '(Error code: ' + IntToStr(BASS_ErrorGetCode) + ')';
	MessageBox(Handle, PChar(s), nil, 0);
end; {Error}

{--------------------------------------------------------------------------------------------------}

procedure TForm1.FormCreate(Sender : TObject);
var
  sf  : BASS_MIDI_FONT;
begin
	if (HIWORD(BASS_GetVersion) <> BASSVERSION) then begin        // check the correct BASS was loaded
		MessageBox(0, 'An incorrect version of BASS.DLL was loaded', nil, MB_ICONERROR);
		Halt;
	end; {if}

	if not BASS_Init(-1, 44100, 0, Handle, nil) then              // Initialize audio - default device, 44100hz, stereo, 16 bits
		Error('Error initializing audio!');

	if (BASS_MIDI_StreamGetFonts(0, PBASS_MIDI_FONT(@sf), 1) >= 1) then
    	fSoundfont  := sf.font;

     // load optional plugins for packed soundfonts (others may be used too)
	BASS_PluginLoad('bassflac.dll', 0);
	BASS_PluginLoad('basswv.dll',  0);
end; {FormCreate}

{--------------------------------------------------------------------------------------------------}

procedure TForm1.FormClose(Sender : TObject; var Action : TCloseAction);
begin

 	BASS_StreamFree(fStream); // Free stream

	BASS_Free;                // Close BASS

end; {FormClose}

{--------------------------------------------------------------------------------------------------}

procedure TForm1.cbEffectsClick(Sender : TObject);
begin
  if cbEffects.Checked then
    BASS_ChannelFlags(fStream, 0,              BASS_MIDI_NOFX)  // enable FX
  else
    BASS_ChannelFlags(fStream, BASS_MIDI_NOFX, BASS_MIDI_NOFX); // disable FX
end; {cbEffectsClick}

{--------------------------------------------------------------------------------------------------}

procedure TForm1.btnOpenClick(Sender : TObject);
var
  bytes : Int64;
  time  : single;
  mark  : BASS_MIDI_MARK;
begin
  with TOpenDialog.Create(Self) do begin

    try

      Filter  := 'MIDI files (mid/midi/rmi/kar)|*.mid;*.midi;*.rmi;*.kar|All files|*.*';

      if Execute then begin

        Update;

        BASS_StreamFree(fStream); // free old stream
        lbLyrics.Caption  := '';  // clear lyrics display

        fStream := BASS_MIDI_StreamCreateFile(false, PChar(FileName), 0, 0, BASS_SAMPLE_LOOP {$IFDEF UNICODE} or BASS_UNICODE {$ENDIF}, 1);

        if (fStream = 0) then begin
          // it ain't a MIDI
          btnOpen.Caption := 'click here to open a file...';
          lbTitle.Caption := '';
          Error('Can"t play the file');

        end else begin

          btnOpen.Caption := FileName;

          // set the title (first text in first track)
          lbTitle.Caption := String(BASS_ChannelGetTags(fStream, BASS_TAG_MIDI_TRACK));

          // update pos scroller range
          bytes           := BASS_ChannelGetLength(fStream, BASS_POS_BYTE);
          time            := BASS_ChannelBytes2Seconds(fStream, bytes);
          TrackBar.Max    := Trunc(time);

          // set lyrics syncs
          if BASS_MIDI_StreamGetMark(fStream, BASS_MIDI_MARK_LYRIC, 0, mark) then         // got lyrics
            BASS_ChannelSetSync(fStream, BASS_SYNC_MIDI_MARK, BASS_MIDI_MARK_LYRIC, @LyricSync, lbLyrics)
          else if (BASS_MIDI_StreamGetMark(fStream, BASS_MIDI_MARK_TEXT, 20, mark)) then  // got text instead (over 20 of them)
            BASS_ChannelSetSync(fStream, BASS_SYNC_MIDI_MARK, BASS_MIDI_MARK_TEXT, @TextSync, lbLyrics);
          BASS_ChannelSetSync(fStream, BASS_SYNC_END, 0, @EndSync, lbLyrics);

          if cbEffects.Checked then
            BASS_ChannelFlags(fStream, 0, BASS_MIDI_NOFX);  // enable FX
            
          BASS_ChannelPlay(fStream, false);

        end; {if}

      end; {if}

    finally

      Free;

    end; {try}

  end; {with}

end; {btnOpenClick}

{--------------------------------------------------------------------------------------------------}

procedure TForm1.btnReplaceClick(Sender : TObject);
var
  sf  : BASS_MIDI_FONT;
begin
  with TOpenDialog.Create(Self) do begin
    try
      Filter  := 'Soundfonts (sf2/sf2pack)|*.sf2;*.sf2pack|All files|*.*';
      if Execute then begin
        Update;
        sf.font := BASS_MIDI_FontInit(PChar(FileName), 0 {$IFDEF UNICODE} or BASS_UNICODE {$ENDIF});  // open new soundfont
        if (sf.font <> 0) then begin
          sf.preset := -1;                                  // use all presets
          sf.bank := 0;                                     // use default bank(s)
          BASS_MIDI_FontFree(fSoundfont);                   // free old soundfont
          BASS_MIDI_StreamSetFonts(0, PBASS_MIDI_FONT(@sf), 1);               // set default soundfont
          BASS_MIDI_StreamSetFonts(fStream, PBASS_MIDI_FONT(@sf), 1);         // set for current stream too
          fSoundfont := sf.font;
        end; {if}
      end; {if}
    finally
      Free;
    end; {try}
  end; {with}
end; {btnReplaceClick}  

{--------------------------------------------------------------------------------------------------}

procedure TForm1.Timer1Timer(Sender : TObject);
var
  FontInfo  : BASS_MIDI_FONTINFO;
begin
  TrackBar.OnChange := nil;
  TrackBar.Position := Trunc(BASS_ChannelBytes2Seconds(fStream, BASS_ChannelGetPosition(fStream, BASS_POS_BYTE))); // update position
  TrackBar.OnChange := TrackBarChange;
  
  if BASS_MIDI_FontGetInfo(fSoundfont, FontInfo) then
    lbSoundfont.Caption := 'name: ' + FontInfo.name + #13#10 +
                           'loaded: ' + Format('%d / %d', [FontInfo.samload, FontInfo.samsize])
  else
    lbSoundfont.Caption := 'no soundfont';
end; {Timer1Timer}

{--------------------------------------------------------------------------------------------------}

procedure TForm1.TrackBarChange(Sender : TObject);
begin
  BASS_ChannelSetPosition(fStream, BASS_ChannelSeconds2Bytes(fStream, TrackBar.Position), BASS_POS_BYTE);
  lbLyrics.Caption  := '';  // clear lyrics
end; {TrackBarChange}

{--------------------------------------------------------------------------------------------------}

function ProcessMarkText(CaptionText, MarkText : string) : string;
var
  StringList  : TStringList;
begin
  case MarkText[1] of
    '@' : Result  := CaptionText;                               // skip info.
    '\' : Result  := Copy(MarkText, 2, pred(Length(MarkText))); // clear line.
	  '/' : begin                                                 // new line
            StringList  := TStringList.Create;
            try
              StringList.Text := CaptionText;
              StringList.Add(Copy(MarkText, 2, pred(Length(MarkText))));
              while (StringList.Count > 3) do
                StringList.Delete(0);
              Result  := Trim(StringList.Text);
            finally
              StringList.Free;
            end; {try}
          end;
  else
    Result  := CaptionText + MarkText;
  end; {case}
end; {ProcessMarkText}

{--------------------------------------------------------------------------------------------------}

procedure LyricSync(Handle : HSYNC; Stream : HSTREAM; data : DWORD; user : Pointer); stdcall;
var
  Mark  : BASS_MIDI_MARK;
begin
  BASS_MIDI_StreamGetMark(Stream, BASS_SYNC_MIDI_LYRIC, data, Mark);
  TLabel(user).Caption  := ProcessMarkText(TLabel(user).Caption, Mark.text);
end; {LyricSync}

{--------------------------------------------------------------------------------------------------}

procedure TextSync(Handle : HSYNC; Stream : HSTREAM; data : DWORD; user : Pointer); stdcall;
var
  Mark  : BASS_MIDI_MARK;
begin
  BASS_MIDI_StreamGetMark(Stream, BASS_MIDI_MARK_TEXT, data, Mark);
  TLabel(user).Caption  := ProcessMarkText(TLabel(user).Caption, Mark.text);
end; {TextSync}

{--------------------------------------------------------------------------------------------------}

procedure EndSync(Handle : HSYNC; Stream : HSTREAM; data : DWORD; user : Pointer); stdcall;
begin
  TLabel(user).Caption  := '';
end; {EndSync}

{--------------------------------------------------------------------------------------------------}

end.
