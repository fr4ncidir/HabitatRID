function varargout = Didar(varargin)
% DIDAR M-file for Didar.fig
%      DIDAR, by itself, creates a new DIDAR or raises the existing
%      singleton*.
%
%      H = DIDAR returns the handle to a new DIDAR or the handle to
%      the existing singleton*.
%
%      DIDAR('CALLBACK',hObject,eventData,handles,...) calls the local
%      function named CALLBACK in DIDAR.M with the given input arguments.
%
%      DIDAR('Property','Value',...) creates a new DIDAR or raises the
%      existing singleton*.  Starting from the left, property value pairs are
%      applied to the GUI before Didar_OpeningFunction gets called.  An
%      unrecognized property name or invalid value makes property application
%      stop.  All inputs are passed to Didar_OpeningFcn via varargin.
%
%      *See GUI Options on GUIDE's Tools menu.  Choose "GUI allows only one
%      instance to run (singleton)".
%
% See also: GUIDE, GUIDATA, GUIHANDLES
% Copyright 2002-2003 The MathWorks, Inc.
% Edit the above text to modify the response to help Didar
% Last Modified by GUIDE v2.5 15-Jul-2011 18:20:28
% Begi initialization code - DO NOT EDIT cc
clc;
gui_Singleton = 1;
gui_State = struct('gui_Name',       mfilename, ...
                   'gui_Singleton',  gui_Singleton, ...
                   'gui_OpeningFcn', @Didar_OpeningFcn, ...
                   'gui_OutputFcn',  @Didar_OutputFcn, ...
                   'gui_LayoutFcn',  [] , ...
                   'gui_Callback',   []);
if nargin && ischar(varargin{1})
    gui_State.gui_Callback = str2func(varargin{1});
end
if nargout
    [varargout{1:nargout}] = gui_mainfcn(gui_State, varargin{:});
else
    gui_mainfcn(gui_State, varargin{:});
end
% End initialization code - DO NOT EDIT
% --- Executes just before Didar is made visible.
function Didar_OpeningFcn(hObject, eventdata, handles, varargin)
% This function has no output args, see OutputFcn.
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
% varargin   command line arguments to Didar (see VARARGIN)
% Choose default command line output for Didar
handles.output = hObject;
% Update handles structure
guidata(hObject, handles);
% UIWAIT makes Didar wait for user response (see UIRESUME)
% uiwait(handles.figure1);
% --- Outputs from this function are returned to the command line.
function varargout = Didar_OutputFcn(hObject, eventdata, handles) 
% varargout  cell array for returning output args (see VARARGOUT);
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% Get default command line output from handles structure
varargout{1} = handles.output;
% --- Executes during object creation, after setting all properties.
function edit1_CreateFcn(hObject, eventdata, handles)
% hObject    handle to edit1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    empty - handles not created until after all CreateFcns called
%       See ISPC and COMPUTER.
if ispc
    set(hObject,'BackgroundColor','white');
else
    set(hObject,'BackgroundColor',get(0,'defaultUicontrolBackgroundColor'));
end

%
%
global type;
type = 0;


% --- Executes on button press in radiobutton1.
function radiobutton1_Callback(hObject, eventdata, handles)
    set(handles.edit1, 'Enable', 'on');
    global type;
    type = 0;
return;

% --- Executes on button press in radiobutton2.
function radiobutton2_Callback(hObject, eventdata, handles)
    set(handles.edit1, 'Enable', 'off');
    set(handles.edit1, 'String', '');
    global type;
    type = 1;
return;

% --- Executes on button press in pushbutton1.
function START_Callback(hObject, eventdata, handles)
    ClearGraphic(handles);
    ClearMessage(handles);
    global type;
    StopGUI(handles);
    tmp = 0;
    type = 1;
    if type == 0
                %%%%%%%%%%%%%%%%%%%%%   DETECT ANCORA DA   TERMINARE   %%%%%%%%%%%%
    else
        set(handles.text1, 'String', 'Point to object....');
        pause(0.01);
        [test, id , file , time1 , time2 , loss] = RID_elaborazione(handles);
        if test == 1
            set(handles.text3, 'ForegroundColor', 'red');
            set(handles.text3, 'String', 'Errore durante la comunicazione con il RID!');
            set(handles.text4, 'String', 'Riavviare la ricerca....');
            beep
            StartGUI(handles);
            return
        elseif test == 2
            set(handles.text3, 'ForegroundColor', 'red');
            set(handles.text3, 'String', 'Errore durante la fase di acquisizione!');
            set(handles.text4, 'String', 'Riavviare la ricerca....');
            StartGUI(handles);
            beep
            return
        elseif test == 3
            set(handles.text3, 'ForegroundColor', 'red');
            set(handles.text3, 'String', 'Errore durante la scansione manuale!');
            set(handles.text4, 'String', 'Riavviare la ricerca....');
            StartGUI(handles);
            return
        end
    end
    new_didar_elaborazione(file,handles);
    set(handles.text3, 'String', 'Elaborazione dei dati terminata con successo');
    time1 = time1*(1/12);
    time2 = time2*(1/12);
    set(handles.text4, 'String', strcat('Clock for cycle: ' , num2str(time1) , 'ms' , ' ; ' , num2str(time2) , 'ms'));
    set(handles.text5, 'String', strcat('loss packet: ' , num2str(loss) , ' pack'));
    %new_didar_elaborazione( tmp , handles );
    StartGUI(handles);
return;

function ID_FORM_Callback(hObject, eventdata, handles)
return;

% --- Executes on button press in pushbutton4.
function LOAD_Callback(hObject, eventdata, handles)
    ClearGraphic(handles);
    ClearMessage(handles);
    StopGUI(handles);
    [FileName,PathName] = uigetfile('.txt','Select data file');
    if (FileName==0) 
        set(handles.text1, 'ForegroundColor', 'red');
        set(handles.text1, 'String', 'Error: File not Found!');
        StartGUI(handles);
        return;
    end
    set(handles.text1, 'String', 'Acquisizione ed elaborazione dati da file....');
    set(handles.text2, 'String', strcat('Elaborato File: ',FileName));
    pause(1);
    new_didar_elaborazione(strcat(PathName,FileName),handles);
    set(handles.text3, 'String', 'Elaborazione dei dati terminata con successo');
    StartGUI(handles)
return;

% --- Executes on button press in pushbutton5.
function CLOSE_Callback(hObject, eventdata, handles)
    set(0,'ShowHiddenHandles','off');
    delete(get(0,'Children'));
return;

function ClearGraphic(handles)
    axes(handles.axes4);
    cla();
    axes(handles.axes5);
    cla();
    axes(handles.axes6);
    cla();
return;

function ClearMessage(handles)
    set(handles.text1, 'String', '');
    set(handles.text2, 'String', '');
    set(handles.text3, 'String', '');
    set(handles.text4, 'String', '');
    set(handles.text5, 'String', '');
    set(handles.text6, 'String', '');
    set(handles.text7, 'String', '');
    set(handles.text1, 'ForegroundColor', 'black');
    set(handles.text2, 'ForegroundColor', 'black');
    set(handles.text3, 'ForegroundColor', 'black');
    set(handles.text4, 'ForegroundColor', 'black');
    set(handles.text5, 'ForegroundColor', 'black');
    set(handles.text6, 'ForegroundColor', 'black');
    set(handles.text7, 'ForegroundColor', 'black');
return;

function StopGUI(handles)
    set(handles.pushbutton1, 'Enable', 'off');
    set(handles.pushbutton4, 'Enable', 'off');
    set(handles.pushbutton5, 'Enable', 'off');
return;

function StartGUI(handles)
    set(handles.pushbutton1, 'Enable', 'on');
    set(handles.pushbutton4, 'Enable', 'on');
    set(handles.pushbutton5, 'Enable', 'on');
return;
