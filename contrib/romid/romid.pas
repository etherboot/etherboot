{ This little program is my try to provide information about the
  EtherBoot rom via environment variables to a batch file. This
  could be done better, I think, but it works...
  The program does not check the environment space yet.
  The TPU code for setting the environment variables I got from an
  archive, it was written by Robert B. Clark <rclark@iquest.net>.
  If someone want to write a better program in C, I also found an
  archive for setting environment variables in C... eflash@gmx.net }

uses environ;

var  c          : byte;
     ah,al,le   : longint;
     i,j,k      : integer;
     s          : string[48];
     fndrom,
     fndpnt,
     verbose,
     walk       : boolean;
     mcb        : MCBType;
     evar,value : evarType;

const
  hexChars: array [0..$F] of Char = '0123456789ABCDEF';
  version = 'v0.53';
  vdate   = '24-06-2000';
  copywr  = 'ROM-ID for Etherboot '+version+' (c) gk '+vdate;


Function  WriteHexWord(w: Word):STRING;
begin
  WriteHexWord := hexChars[Hi(w) shr 4] +
        hexChars[Hi(w) and $F] +
        hexChars[Lo(w) shr 4] +
        hexChars[Lo(w) and $F];
end;

Function  WriteHexByte(w: Byte):STRING;
begin
  WriteHexByte := hexChars[w shr 4] +
        hexChars[w and $F];
end;

procedure err;
begin
  writeln(^g'Environment setting fails!');
  exit;
end;

{ **************** main stuff **************** }

begin
  assign(output,'');
  append(output);
  writeln(^j,copywr,^m^j);
  verbose:=false;
  IF ParamCount>0 then
    begin
      s:=ParamStr(1);
      if upcase(s[2])='V' then verbose:=true
      else
         begin
           writeln('ROMID [-v]   verbose');
           exit;
         end;
    end;
  fndpnt:=false;
  le:=$10;
  ah:=$c800;
  s:='';
  repeat
    if ((mem[ah:0000]=$55) and (mem[ah:0001]=$AA)) then le:=mem[ah:0002];
    al:=512*le-3;
    for i:=0 to 5 do
      begin
        c:=mem[ah:al-i];
        if c=$2E then
          begin
            al:=al-i;
            fndpnt:=true;
            break;
          end;
      end;
  ah:=ah+$20*le;
  until (fndpnt or (ah>=$e000));
  ah:=ah-$20*le;
  s:='';
  if fndpnt then
    begin
      if verbose then writeln('Rom found at   : ',WriteHexWord(ah));
      if verbose then writeln('Rom size is    : ',WriteHexWord(512*le),' -> ',le div 2,'kByte');
      for i:=1 to 40 do
        begin
          c:=mem[ah:al-i];
          if c=$FF then break;
        end;
      if c=$FF then
        begin
          for i:=i-1 downto 1 do
            begin
              c:=mem[ah:al-i];
              s:=s+chr(c);
            end;
          if verbose then writeln('ROM-Type String: ',s);
          i:=1;
          repeat
            c:=mem[ah:al-i];
            inc (i);
          until ((c=$20) or (c=$2F));
          s:='';
          for i:=i-2 downto 1 do
            begin
              c:=mem[ah:al-i];
              s:=s+upcase(chr(c));
            end;

            FindRootEnv(mcb);
            evar:='ROMID'; value:=s;
            if not PutEnv(evar,value,MASTER_ENVSEG) then err;
            s:='';
            for i:=0 to 2 do
              begin
                c:=mem[ah:$001c+i];
                s:=s+chr(c);
              end;
            if s='PCI' then
              begin
                s:='';
                k:=mem[ah:$0020]+256*mem[ah:$0020+1];
                s:=WriteHexWord(k)+':';
                k:=mem[ah:$0020+2]+256*mem[ah:$0020+3];
                s:=s+WriteHexWord(k);
                if verbose then writeln('ROM-ID PCI-Bios: ',s);
                evar:='PCIID'; value:=s;
                if not PutEnv(evar,value,MASTER_ENVSEG) then err;
              end
            else writeln('No PCI-Identifier found.');
         end;
       end
     else writeln('No EtherBoot-ROM found.');
end.
