const n=9;
      k=0;
      n_gen = n*(n-1) div 2 - k;
 {a: array[0..n-1] of byte;}
type g_arr = array[0..n_gen-1] of byte;
var    g: g_arr{ = (0,3,1,4,0,2,1,3,0,2,4,1,3,0,2);};
var flag: boolean;

function A: integer;
var i, s: integer;
begin
     s:=0;
     for i:= 0 to n_gen-1 do
         dec(s, (g[i] and 1)*2-1);
     a:=abs(s-1 + n and 1);
end;

function odd(const x: integer):boolean;
begin
     odd := flag xor boolean(x and 1);
end;

procedure commutate(const i, iSos, j: integer);
var old, k: integer;
begin
     old := g[i];
     for k:= i to iSos - 1 do
         g[k] := g[k+1];
     for k:= j-1 downto iSos + 1 do
         g[k+1] := g[k];
     g[iSos-1] := g[iSos];
     g[iSos+1] := g[iSos];
     g[iSos] := old;
end;

function go:boolean;
var i, j: integer;
    iSos, sos: integer;
begin
     for i:= 0 to n_gen-1 do
         begin
         if odd(g[i]) then
            begin
            sos := 0;
            for j:= i+1 to n_gen-1 do
                begin
                if g[i]=g[j] then
                   begin
                   if sos = 1 then
                      begin
                      commutate(i, iSos, j);
                      go := true;
                      exit;
                      end
                   else
                       break;
                   end;
                if abs(g[j]-g[i]) = 1 then
                   begin
                   iSos := j;
                   inc(sos);
                   end;
                end;
            end;
         end;
     go := false;
end;

procedure print_g;
var q: integer;
begin
     for q:=0 to n_gen - 1 do
         write(g[q], ' ');
     writeln;
end;

procedure shift(const s: integer);
var t: g_arr;
    i: integer;
begin
for i:=0 to n_gen-1 do
    t[i] := g[i];
for i:=0 to s-1 do
    g[i] := n - 2 - g[n_gen+i-s];
for i := n_gen-1 downto s do
    begin
    g[i] := t[i-s];
    end;
end;

var i, j, q, ma, a1: integer;
begin
     q := 0;
     ma := 0;
     writeln;
     writeln;
     writeln;
     writeln;
     for i:=0 to n-2 do
     for j:=i downto 0 do
         begin
         g[q] := j;
         inc(q);
         end;
     flag := false;

     while true do
     begin
     flag := not flag;
     while go do
           print_g;
     flag := not flag;
     while go do
           print_g;
     flag := not flag;
     while go do
           print_g;
     shift(1);

     a1 := a;
     if ma < a1 then ma := a1;
     writeln('A=', a1, ' Max: ', ma);
     readln;
     end;
end.