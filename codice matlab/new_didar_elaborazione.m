function didar_elaborazione(sFileName,handles)
%%%%%%%%%%% Load File %%%%%%%%%%%%%%%%
dati = load(sFileName);

%%%%%%%%%%% Impostazioni iniziali %%%%%%%%%%%%%%%%

% Dimensioni vettori 
[nSM,nED] = size(dati);
nED = nED/2;
nSM = nSM-1;
pts = 1:nSM;

% impostazioni per grafici
spess = 1;
ncol = nED*2;
colorm = hsv(ncol);
color1 = colorm(1:2:ncol,:);
color2 = colorm(2:2:ncol,:);

%%%%%%%%%%% Carica i dati da file %%%%%%%%%%%%%%%%

% Dimensioni dei vettori
S0 = zeros(nSM,nED);
D0 = zeros(nSM,nED);
sEDNames = cell(nED,1);

% Lettura da file
for j=1:nED
    % Legge gli id
    sEDNames{j} = num2str(dati(1,(j-1)*2+1));
    
    % Legge le misure fatte da somma
    S0(:,j) = dati(2:end,(j-1)*2+1);
    
    % Legge le misure fatte da differenza
    D0(:,j) = dati(2:end,j*2);
end

%%%%%%%%%%% Calcolo monopulse formula %%%%%%%%%%%%%%%%

% denoise dei dati
[S1,D1] = denoise(nED,nSM,-200,S0,D0);

% calcolo ...
[S2,D2] = mpratio(nED,nSM,S1,D1);

% Detrmina il tag che ha max mpratio prossimo all'origine
[win, S2] = TheWinnerIs(nED, nSM, S2);

%%%%%%%%%%% Costruzione grafici %%%%%%%%%%%%%%%%

% determina gli assi
ymn = min( floor(min(min(S1))/5)*5 , floor(min(min(D1))/5)*5 );
ymx = max(ceil(max(max(S1))/5)*5,ceil(max(max(D1))/5)*5);
ymx = min(-40,ymx);

% Grafica gli assi
Grafica(handles.axes4,handles.axes5,1,true,true,ymn,ymx,lines,nED,pts,S0,D0,color1,1,sEDNames,'Original',win);
Grafica(handles.axes4,handles.axes5,2,true,true,ymn,ymx,lines,nED,pts,S1,D1,color2,2,sEDNames,'Enhanced',win);

% determina gli assi
ymn = floor(min(min(S2))/5)*5;
ymx = ceil(max(max(S2))/5)*5;

% Grafica gli assi
Grafica(handles.axes6,handles.axes6,3,true,false,ymn,ymx,lines,nED,pts,S2,D2,color2,2,sEDNames,'Monopulse Ratio',win);

return

function [j , S0] = TheWinnerIs(nED, nSM, S0)
    zeroPos = nSM/2;
    d=zeros(1,nED);
    
    for i=1:1:nED
        %if test(i) == 1
            [value,index] = max(S0(:,i));
            d(i) = abs(index - zeroPos);
        %end
    end
    
    [value , j]= min(d);
    for i=1:nSM
    	S0(i,j) = S0(i,j);
    end
    
return

function [S1,D2] = mpratio(nED,nSM,S0,D0)
    S1 = S0;
    D1 = D0;
    D2 = D0;
    S1(:) = 0;
    D1(:) = 0;
    
    MAX_LIM = 90;
    MIN_LIM = -100;
    
    for j=1:nED
        for i=1:nSM
        	S1(i,j) = S0(i,j)-mean(S0(:,j))-std(S0(:,j));
         	D1(i,j) = D0(i,j)-mean(D0(:,j))+std(D0(:,j));
          	S1(i,j) = (S1(i,j)-D1(i,j));
        end
        tmp = smooth(S1(:,j),0.2,'loess');%S1(:,j);
        S1(:,j) = tmp;
    end    
return

function [S2, D2] = denoise(nED,nSM,thr,S0,D0)
    nMean = 2;
    S1 = S0;
    D1 = D0;
    S2 = S0;
    D2 = D0;
    for j=1:nED
        markStart = 0;
        markEnd = 0;
        for i=1:nSM
            if (S0(i,j) == 0) && (i<nSM)
                markEnd = i;
            else
                if markStart < markEnd
                    if S0(markEnd+1,j) == 0
                        if markEnd == nSM-1
                            markEnd = nSM;
                        end
                        yE = mean(S0(:,j));
                    else
                        yE = S0(markEnd+1,j);
                    end
                    if markStart < 2
                        markStart = 1;
                        yS = mean(S0(:,j));
                    else
                        yS = S0(markStart,j);
                    end
                    for h=(markStart+1):markEnd
                        S1(h,j) = yS+(h-markStart)*(yE-yS)/(markEnd-markStart);
                    end
                end
                markStart = i;
                markEnd = i;
            end
        end          
        markStart = 0;
        markEnd = 0;        
        for i=1:nSM
            if (D0(i,j) == 0) && (i<nSM)
                markEnd = i;
            else
                if markStart < markEnd
                    if D0(markEnd+1,j) == 0
                        if markEnd == nSM-1
                            markEnd = nSM;
                        end
                        yEE = mean(D0(:,j));
                    else
                        yEE = D0(markEnd+1,j);
                    end
                    if markStart < 2
                        markStart = 1;
                        ySS = mean(D0(:,j));
                    else
                        ySS = D0(markStart,j);
                    end
                    for h=(markStart+1):markEnd
                        D1(h,j) = ySS+(h-markStart)*(yEE-ySS)/(markEnd-markStart);
                    end
                end
                markStart = i;
                markEnd = i;
            end
        end
    end

    S2 = S1;
    D2 = D1;
%    Questa versione associa il valore di media mobile che trova 
%    utilizzando i valori dei dati limitrofi (fiestra mobile
%    di dimensione nMean = 6)
    nMean = 6;
    for j=1:nED
        for i = (nMean/2+1):(nSM-nMean/2)
            i-nMean/2;
            S2(i,j) = 0.05*S1(i-3,j)+0.1*S1(i-2,j)+0.15*S1(i-1,j)+0.4*S1(i,j)+0.15*S1(i+1,j)+0.1*S1(i-2,j)+0.05*S1(i+3,j);
            % Controlla che il valore non sia al di sotto di una
            % determinata soglia. In tal caso lo scarta.
            tmp = D1((i-nMean/2) : (i+nMean/2),j);
                for h=1:size(tmp);
                    if (tmp(h)<thr)
                        tmp(h) = 0;
                    end;
                end;
            D2(i,j) = 0.05*tmp(1)+0.1*tmp(2)+0.15*tmp(3)+0.4*tmp(4)+0.15*tmp(5)+0.1*tmp(6)+0.05*tmp(7);
        end
        for i = nSM-nMean/2+1:nSM
            S2(i,j) = sum(S1(i:end,j))/(nSM-i+1);
            D2(i,j) = sum(D1(i:end,j))/(nSM-i+1);
        end
        for i = 1:nMean/2
            S2(i,j) = sum(S1(1:i,j))/i;
            D2(i,j) = sum(D1(1:i,j))/i;
        end
    end

return

function Grafica(hS,hD,ipl,first, second,ymn,ymx,lines,nED,pts,S,D,colorm,spess,sEDNames,name,win)
if first
    axes(hS);
    for j=1:nED
        if j==win && ipl==3
            plot(pts, S(:,j),'Color',colorm(j,:),'Linewidth',spess*2);
        else
            plot(pts, S(:,j),'Color',colorm(j,:),'Linewidth',spess);
        end
        hold('all');
    end
    title(strcat('S-',name));
    if (ipl == 1) ylabel('dBm'); end;
    ylim([ymn ymx]);
    xlim([min(pts) max(pts)]);
    grid on;
    if (ipl == 3)
        legend(hS,sEDNames,'Location','SouthWest');
    end
    for j=1:size(lines)
        plot([lines(j) lines(j)],[ymn ymx],'Color', [0 0 0]);
    end;
end

if second
    axes(hD);
    for j=1:nED
        plot(pts, D(:,j),'Color',colorm(j,:),'Linewidth',spess);
        hold('all');
    end
    title(strcat('D-',name));
    if (ipl == 1) ylabel('dBm'); end;
    ylim([ymn ymx]);
    xlim([min(pts) max(pts)]);
    grid on;
    if (ipl == 3)
        legend(hD,sEDNames,'Location','SouthWest');
    end
    for j=1:size(lines)
        plot([lines(j) lines(j)],[ymn ymx],'Color', [0 0 0]);
    end;
end  
return

function str = str2id(id)
    if(id==100) 
            str = ' Air Condition.'; % luce 1
    elseif(id==101)
        str = ' Fan'; % botola
    elseif(id==102)
        str = ' Water Leak';
    elseif(id==103) 
        str = ' Radiator'; % luce 2
    elseif(id==104)
        str = ' Lighting'; % luce emergenza
    end;
return;