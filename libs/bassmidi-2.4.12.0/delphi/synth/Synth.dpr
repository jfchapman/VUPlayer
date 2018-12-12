program synth;

uses
  Forms,
  Controls,
  Main in 'Main.pas' {MainForm},
  Bass in '..\Bass.pas',
  BassMIDI in '..\bassmidi.pas';

begin
  Application.Initialize;
  Application.CreateForm(TMainForm, MainForm);
  Application.Run
end.
