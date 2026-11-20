@echo off
chcp 65001 >nul
rem Visual Studio가 없을 때 gcc(MinGW-w64)로 빌드하는 파일입니다.
rem 이 파일을 더블클릭하면 같은 폴더에 LibraryProgram.exe 가 만들어집니다.
cd /d "%~dp0"

where gcc >nul 2>nul
if errorlevel 1 (
    echo gcc를 찾을 수 없습니다. MinGW-w64를 설치하고 PATH에 추가한 뒤 다시 실행하세요.
    pause
    exit /b 1
)

set SOURCES=src\main.c src\ui.c src\page_home.c src\page_search.c src\page_loan.c src\page_seat.c src\page_admin.c src\page_student.c src\user.c src\search.c src\loan.c src\seat.c src\file.c src\admin.c src\crypto.c

rem res\app.rc : 버튼과 표를 최신 윈도우 모양으로 보이게 하는 설정 (windres가 없으면 건너뜀)
set RESOURCE=
where windres >nul 2>nul
if not errorlevel 1 (
    windres res\app.rc -O coff -o res\app.res
    if not errorlevel 1 set RESOURCE=res\app.res
)

gcc -std=c99 -O1 -finput-charset=UTF-8 %SOURCES% %RESOURCE% -o LibraryProgram.exe -mwindows -lcomctl32 -lgdi32
if errorlevel 1 (
    echo 빌드에 실패했습니다. 위의 오류 메시지를 확인하세요.
    pause
    exit /b 1
)

echo 빌드 성공: LibraryProgram.exe
pause
