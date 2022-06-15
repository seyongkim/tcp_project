#include <stdio.h>  //헤더파일을 선언합니다.
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 100 //버퍼 사이즈를 정의합니다.
#define NAME_SIZE 20  //이름의 사이즈를 정의합니다.

void * send_msg(void * arg);  //메세지를 보내는 함수를 정의합니다.
void * recv_msg(void * arg);  //메세지를 받는 함수를 정의합니다.
void error_handling(char * msg); //에러를 제어할 수 있는 에러 핸들링 함수를 정의합니다.

char name[NAME_SIZE]="[DEFAULT]"; //채팅자 이름을 받을 수 있는 배열을 선언합니다, 본인 닉네임을 20자로 제한한다.
char msg[BUF_SIZE]; //메세지의 배열을 선언합니다.

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in serv_addr; 
	pthread_t snd_thread, rcv_thread;
	void * thread_return;
	if(argc!=4) { 
	//입력에 대한 오류처리입니다.(IP, 포트 입력 확인)
		printf("Usage : %s <IP> <port> <name>\n", argv[0]);
		exit(1);
	}
	sprintf(name, "[%s]", argv[3]); //채팅자의 이름을 설정한다. ex) argv(3)이 '세용'이라면 "[세용]"이 name이 된다
	sock=socket(PF_INET, SOCK_STREAM, 0); // 소켓 함수를 호출한다.

	memset(&serv_addr, 0, sizeof(serv_addr)); //소켓에 대한 설정을 한다.
	serv_addr.sin_family=AF_INET; //IPv4를 사용한다는 의미이다.
	serv_addr.sin_addr.s_addr=inet_addr(argv[1]); //서버의 IP를 입력한다.
	serv_addr.sin_port=htons(atoi(argv[2])); // Port를 입력한다.

	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) //서버 연결을 요청하는 부분이다. 
		error_handling("connect() error");							      /*connect함수의 매개변수로는 파일 디스크립터, 
																		   연결 요청을 보낼 서버의 주소정보를 지닌 구조체 변수의 포인터,
																		   serv_addr포인터가 가리키는 주소 정보 구조체의 크기가 있다.*/
		
	//두 개의 쓰레드 생성하고, 각각의 main 은 send_msg, recv_meg이다.
	pthread_create(&snd_thread, NULL, send_msg, (void*)&sock); //send 메세지 쓰레드를 생성한다.
	pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock); //receive 메세지 쓰레드를 생성한다.
	// 쓰레드 종료 대기 및 소멸 유도하는 부분이다.
	pthread_join(snd_thread, &thread_return); // 쓰레드 send 메세지 ID를 생성한다.
	pthread_join(rcv_thread, &thread_return); // 쓰레드 receive 메세지 ID를 생성한다.
	close(sock); //소켓을 종료한다.
	return 0;
}

void * send_msg(void * arg) // send 메세지 함수이다.
{
	int sock=*((int*)arg); //호출 받은 소켓을 연결한다, void형 int형으로 전환
	char name_msg[NAME_SIZE+BUF_SIZE]; //메세지의 배열 사이즈를 선언한다, 사용자 아이디와 메세지를 붙여서 한번에 보낸다.
	while(1) //메세지 입력부분
	{
		fgets(msg, BUF_SIZE, stdin);//입력받음
		if(!strcmp(msg, "q\n")||!strcmp(msg, "Q\n"))//Q입력 시 종료
		{
			//서버에 EOF를 보낸다.
			close(sock);
			exit(0);
		}
		// id 를 "세용", msg 를 "짱" 으로 했다면, => [세용] 짱
    	// 이것이 name_msg 로 들어가서 출력됨
		sprintf(name_msg, "%s %s", name, msg); //서버로 메세지를 보낸다.
		write(sock, name_msg, strlen(name_msg)); //메시지 소켓을 출력한다.
	}
	return NULL;
}

void * recv_msg(void * arg) // read thread main, receive message function
{
	int sock=*((int*)arg); //호출 받은 소켓 연결
	char name_msg[NAME_SIZE+BUF_SIZE];// 메세지 배열을 선언
	int str_len; //메세지 길이를 선언한다.
	while(1)
	{
		str_len=read(sock, name_msg, NAME_SIZE+BUF_SIZE-1); //메세지를 수신하는 부분이다.
		 /* str_len 이 -1 이라는 건, 서버 소켓과 연결이 끊어졌다는 뜻이다.
   		 왜 끊어졌는가? send_msg 에서 close(sock) 이 실행되고,
		서버로 EOF 가 갔으면, 서버는 그걸 받아서 "자기가 가진" 클라이언트 소켓을 close 할 것
   		그러면 read 했을 때 결과가 -1 일 것.*/
		if(str_len==-1)
			return (void*)-1; //pthread_join을 실행하기위해
		name_msg[str_len]=0; //버퍼 맨 마지막 값이 NULL이다.
		fputs(name_msg, stdout); //메세지를 출력한다.
	}
	return NULL;
}

void error_handling(char *msg) //에러를 제어하는 함수
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
