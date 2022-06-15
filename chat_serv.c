#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUF_SIZE 100 //버퍼 사이즈를 정의한다.
#define MAX_CLNT 256 // 최대 동시 접속자 수를 정의한다.

void * handle_clnt(void * arg);
void send_msg(char * msg, int len);
void error_handling(char * msg);
//접속한 클라이언트 수
int clnt_cnt=0;
// 접속할 클라이언트의 수가 불확실하므로 클라이언트 소켓은 배열로 선언한다.
// 멀티쓰레드 시, 이 두 전역변수, clnt_cnt, clnt_socks 에 여러 쓰레드가 동시 접근할 수 있기에 
// 두 변수가 사용된다면 임계영역이라고 볼 수 있다.
int clnt_socks[MAX_CLNT]; //최대 256명의 클라이언트
pthread_mutex_t mutx; //mutex를 선언한다. 다중 스레드 사용시 혼잡을 막기 위해.

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	int clnt_adr_sz;
	pthread_t t_id; //스레드를 선언한다.


	if(argc!=2) {     //파일명이나 포트번호를 확인해 오류처리를 한다.
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	pthread_mutex_init(&mutx, NULL);//뮤텍스를 생성한다.
	serv_sock=socket(PF_INET, SOCK_STREAM, 0);


	//IPv4, IP, Port를 할당한다.
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET;
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_adr.sin_port=htons(atoi(argv[1]));
	//주소를 할당한다.
	if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr))==-1)
		error_handling("bing() error");
	
	if(listen(serv_sock, 5)==-1)			
		error_handling("listen() error");	 /* 5로 값을 지정되었으니 클라이언트 수 제한이 5라고 착각할 수 있다.
											 이것은 큐의 크기를 의미할 뿐이고 운영체제에 부담이 없다면 256명까지
											 동시접속이 가능하다.*/
	//종료조건이 딱히 없다. ctrl + c로 종료한다.
	while(1)
	{ 
		clnt_adr_sz=sizeof(clnt_adr);
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr,&clnt_adr_sz);

		pthread_mutex_lock(&mutx); //전역변수를 사용하기 위해 mutex를 lock 상태로 전환한다.
		clnt_socks[clnt_cnt++]=clnt_sock;//클라이언트 카운트를 올리고 첫번 째 클라이언트는  clnt_socks[0]에 들어간다
		pthread_mutex_unlock(&mutx);//mutex를 unlock상태로 전환한다.

		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock); /*쓰레드를 생성하고 ,handle_clnt함수를 실행한다
    handle_clnt 에서 파라미터로 받을 수 있도록 함*/
		pthread_detach(t_id); //쓰레드를 소멸시킨다.
		printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));//inet_ntoa함수는 int32형을 문자열 값으로 보여준다.
	}
	close(serv_sock);//ctrl + c로 프로그램을 종료 시키면 서버 소켓도 종료된다.
	return 0;
}

void * handle_clnt(void * arg)
{
	//파일 디스크립터가 void형이므로 int형으로 변환한다.
	int clnt_sock=*((int*)arg);
	int str_len=0, i;
	char msg[BUF_SIZE];

	while((str_len=read(clnt_sock, msg, sizeof(msg)))!=0) // 클라이언트에서 보낸 메세지 받음.
  // 클라이언트에서 EOF 보내서, str_len 이 0이 될때까지 반복
  // EOF 를 보내는 순간은 언제인가? 클라이언트에서, 소켓을 close 했을 때이다!
  // 즉, 클라이언트가 접속을 하고 있는 동안에, while 문을 벗어나진 않는다
		send_msg(msg, str_len);
		//while 문을 탈출할 경우 소켓의 연결이 끊어졌다는 것을 의미, 따라서 clnt_socks[]와 쓰레드 삭제한다.
	pthread_mutex_lock(&mutx);//전역변수인 clnt_cnt와 clnt_socks[]를 사용할 예정이므로 mutex를 lock상태로 전환한다.
	for(i=0; i<clnt_cnt; i++) // remove disconnected client
	{
		if(clnt_sock==clnt_socks[i])
		{
			while(i++<clnt_cnt-1) //현재 소켓이 위치했던 자리 기준으로 뒤의 클라이언트를 땡겨서 가져온다.,쓰레드 한개를 삭제
				clnt_socks[i]=clnt_socks[i+1]; 
			break;
		}
	}	
	clnt_cnt--; //클라이언트 수를 하나 줄인다.
	pthread_mutex_unlock(&mutx); //mutex를 unlock상태로 전환
	close(clnt_sock);//서버 쓰레드의 클라이언트 소켓을 종료시킨다
	return NULL;
}
void send_msg(char * msg, int len) // send to all, 접속한 모두에게 메세지를 보낸다
{
	int i;
	pthread_mutex_lock(&mutx); //clnt_cnt, clnt_socks[] 사용을 위해 mutex를 lock상태로 전환한다.
	for(i=0; i<clnt_cnt; i++)
		write(clnt_socks[i], msg, len); //모든 클라이언트에게 메세지를 보낸다
	pthread_mutex_unlock(&mutx); //mutex를 unlock상태로 전환한다.
}
void error_handling(char * msg) //에러를 제어하는 함수이다.
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
