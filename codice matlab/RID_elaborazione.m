function [test, id , file , time1 , time2 , loss] = RID_elaborazione(handles)
    %PORTA PC MASOTTI: 'COM13'
    %PORTA PC CASA: 'COM6'
    %PORTA PC portatile: 'COM3'
    %PORTA PC PAOLO: 'COM4'
    %PORTA PC LAB: 'COM7'
    %versione pda_command(tag statici)
    s = serial( 'COM7' , 'BaudRate' , 115200, 'Terminator' , 'LF');
    set( s , 'Timeout' , 2 );
    fopen(s);
    tmp=''; 
    tmp = strcat(tmp,'M_',datestr(now,'dd-mmm-yyyy-HH_MM_SS'),'_P000.txt');
    file = tmp;
    time1 = 0;
    time2= 0;
    loss = 0;
    id=0;
    test = 0;
    fprintf(s,'%s','<');
    line=fgets(s);
    if (isempty(line)) 
        test = 1;
        fprintf(s,'%s','+');
        fclose(s);
        return; 
    end;
    set( s , 'Timeout' , 1.2 );
    line=fgets(s);
    if (isempty(line)) 
        test = 2;
        fprintf(s,'%s','+');
        fclose(s);
        return; 
    end;
    set(handles.text2, 'String', 'Selection successfully started...');
    norm_line=int16(line);
    Len0 = norm_line(1);
    id = norm_line;
    str = num2str(norm_line(1));
    pause(0.05);
    beep
    dlmwrite(tmp,'','Precision','%5i','delimiter',' ');
    dlmwrite(tmp,norm_line(1,2:(2*norm_line(1)+1)),'Precision','%5i','delimiter',' ','-append');
    h = waitbar(0,'No.0 received bursts...','Name',' RSSI measurements');
    set( s , 'Timeout' , 0.5 );
    steps = 40;
    beep
    loss = 0;
    for step=1:steps
        fprintf(s,'%s','<');
        norm_line=int16(fgets(s));
        if (length(norm_line) < Len0) 
            if (step ~= 1)
                if (step > 25)
                    norm_line = zeros(1,2*Len0);
                    dlmwrite(tmp,norm_line,'Precision','%5i','delimiter',' ','-append');
                    str = num2str(step);
                    str = strcat('N. ', str, ' received bursts...');
                    waitbar(step/steps,h,sprintf(str));      
                    if (step==48) 
                        fprintf(s,'%s','+'); 
                    end
                else
                    test=3;
                    fprintf(s,'%s','+');
                    break;
                end
            end
            test=3;
            fprintf(s,'%s','+');
            break;
        end
        len = length(norm_line);
        for i=1:1:len-1
            if norm_line(i) == 255
                norm_line(i) = 0;
                loss = loss + 1;
            else
                norm_line(i) =  norm_line(i) - 256;
            end
        end
        norm_line = norm_line(1,1:len-1);
        dlmwrite(tmp,norm_line,'Precision','%5i','delimiter',' ','-append');
        str = num2str(step);
        str = strcat('N. ', str, ' received bursts...');
        waitbar(step/steps,h,sprintf(str));
        pause(0.01);
    end
    beep
    if test ~= 3 
        fprintf(s,'%s','>');
        fgets(s);
    end;
    %time1 = double(fgetl(s))
    %time2 = double(fgetl(s))
    fclose(s);
    delete(h);
return

function str = str2id(id)
    if(id==100) 
            str = ' Light N.1';
    elseif(id==101)
        str = ' Light N.2';
    elseif(id==102)
        str = ' Emergency';
    elseif(id==103) 
        str = ' Valve';
    elseif(id==104)
        str = ' Light N.3';
    else
        str = 'error';
    end;
return;
    