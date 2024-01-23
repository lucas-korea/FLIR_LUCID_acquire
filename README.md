<center>취득 프로그램을 이용한 취득 장면</center>

## 버전 정보 
- [Arena sdk](https://thinklucid.com/downloads-hub/) 1.0.36.7  
- [Spinnaker](https://www.flirkorea.com/products/spinnaker-sdk/?vertical=machine+vision&segment=iis) 2.3.0.77  
- Opencv 1.4.3  
- Visual studio 2022  

## 구현 기능
 - FLIR 카메라(일반 가시광), LUCID 카메라(편광) 동시 취득 기능
 - 센서별 동작 상태 확인기능
 - 실시간 모니터링 기능
 - 취득 로그 저장기능

## 준비 및 동작
 1. 버전 정보에 해당하는 sdk를 미리 설치해야함  
 2. opencv dll은 해당 폴더에 옮기던지, 참조를 해야함  
 3. FLIR, LUCID 카메라가 연결되어 있어야 함 (없어도 동작은 됨)
 4. test2.sln 파일을 열어 run, 혹은 동봉된 test2.exe 파일을 실행

## 함수 설명

#### AcquireImagesFLIR  

```DWORD WINAPI AcquireImagesFLIR(LPVOID lpParam)```
 - FLIR camera를 취득하기 위한 thread 함수이다. WINODW API로 구현되어 있음
 - 카메라 상태체크, 카메라 시동, 데이터 취득, 카메라 연결 해제 기능 구현
 ***
#### AcquireImagesLUCID  
```DWORD WINAPI AcquireImagesLUCID(LPVOID lpParam)```
 - LUCID camera를 취득하기 위한 thread 함수이다. WINODW API로 구현되어 있음
 - 카메라 상태체크, 카메라 시동, 데이터 취득, 카메라 연결 해제 기능 구현
 ***
#### CreateFiles  
```int CreateFiles(string serialNumber, int cameraCnt, string CurrentDateTime_)```
 - 취득될 카메라 데이터, 로그파일을 생성
 ***
#### getCurrentDateTime 
 - log 파일 및 파일명에 사용할 현재날짜 불러오기
 *** 
#### getTimeStamp  
 - log 파일 및 파일명에 사용할 현재날짜 불러오기
 ***
#### ImageRotateInner  
```cv::Mat ImageRotateInner(const cv::Mat src, double degree```
 - 이미지를 뒤집어 주는 함수(장착 이슈로 카메라가 거꾸로 되어있음)
  ***
#### MonitoringThread 
 - 카메라 취득을 확인하기 위한 카메라 데이터 실시간 뷰어   - window api thread로 구현
 ***
#### readconfig  
 - 카메라 시동 전 설정하기 위한 myconfig.txt 파일을 읽어 설정 값을 변수에 저장
 ***
#### RunMultipleCameras  
 - 카메라 취득, 모니터링 함수들의 쓰레드를 관리하고 시작하는 중요 함수  
  
<p align="center">
	<img src="https://github.com/lucas-korea/FLIR_LUCID_acquire/assets/57425658/6578ca78-78d5-4024-9487-01b426f041db"  width="300" height="170">
	<em>취득 프로그램 동작 장면</em>
<p>

<p align="center">
	<img src="https://github.com/lucas-korea/FLIR_LUCID_acquire/assets/57425658/2bf5a78e-8b37-44ab-87b9-e36c3297f125"  width="300" height="170">
	<em>취득 프로그램을 이용한 취득 장면</em>
<p>
