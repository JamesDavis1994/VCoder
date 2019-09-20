#define Version "2.2"

[Setup]
AppId={{52B5C6AF-5E2B-4BDB-94A4-8CB23E8D69DB}}
AppName=Voukoder (COM Server)
AppVersion={#Version}
AppVerName=Voukoder (COM Server) {#Version}
AppPublisher=Daniel Stankewitz
AppPublisherURL=https://www.voukoder.org
AppSupportURL=https://www.voukoder.org/forum/
AppUpdatesURL=https://github.com/Vouk/voukoder/
DefaultDirName={localappdata}\Voukoder\
DisableProgramGroupPage=yes
DisableDirPage=yes
LicenseFile=C:\Users\Daniel\source\repos\voukoder\LICENSE
OutputBaseFilename=voukoder-comserver
Compression=lzma
SolidCompression=yes
VersionInfoVersion={#Version}
WizardSmallImageFile=logo.bmp

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "catalan"; MessagesFile: "compiler:Languages\Catalan.isl"
Name: "corsican"; MessagesFile: "compiler:Languages\Corsican.isl"
Name: "czech"; MessagesFile: "compiler:Languages\Czech.isl"
Name: "danish"; MessagesFile: "compiler:Languages\Danish.isl"
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "finnish"; MessagesFile: "compiler:Languages\Finnish.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "hebrew"; MessagesFile: "compiler:Languages\Hebrew.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "norwegian"; MessagesFile: "compiler:Languages\Norwegian.isl"
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "slovenian"; MessagesFile: "compiler:Languages\Slovenian.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "ukrainian"; MessagesFile: "compiler:Languages\Ukrainian.isl"

[Files]
Source: "..\x64\Release\voukoder.dll"; DestDir: "{app}"; Flags: regserver 64bit;
