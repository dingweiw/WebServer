#include "lst_timer.h"
#include "../http/http_conn.h"


/*
信号是一种异步事件：信号处理函数和程序的主循环是两条不同的执行路线。信号处理函数需要尽可能快地执行完毕，
以确保该信号不被屏蔽太久。其典型解决方案是：把信号的主要处理逻辑放到程序的主循环中，当信号处理函数被触发时，
它指示简单的通知主循环程序接收信号，并将信号值传递给主循环。主循环再根据接收到的信号值执行目标信号对应的逻辑代码。
信号处理函数通常使用管道将信号值传递给主函数，主函数通过I/O服用系统调用来监听管道读端文件描述符上的可读事件
即可统一事件源。
*/



sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}

sort_timer_lst::~sort_timer_lst()
{
    util_timer *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

// 将目标定时器添加到链表中，将定时器按超时时间升序插入
void sort_timer_lst::add_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    if (!head) // 如果当前链表中只有头尾节点
    {
        head = tail = timer; // 直接插入
        return;
    }
    if (timer->expires < head->expires) // 如果新的定时器的超时时间小于当前头部节点
    {
        // 直接将当前定时器节点作为头结点
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer((timer, head)); // 其他情况调用私有成员，重新插入
}

//任务发生变化时，调整该定时器在链表中的位置
void sort_timer_lst::adjust_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    util_timer *tmp = timer->next;
     // 被调整的定时器是链表的尾部节点 或 定时器超时值仍然小下一个定时器超时值
    if (!tmp || (timer->expires < tmp->expires))
    {
        return; //不调整
    }
   // 被调整定时器是头部节点
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    // 被调整定时器在链表内部
    else
    {
        // 将定时器取出
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        // 调用私有成员，重新插入
        add_timer(timer, timer->next);
    }
}

// 删除定时器
void sort_timer_lst::del_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    // 链表只有一个定时器
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    // 被删除的定时器是头结点
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    // 被删除的定时器是尾结点
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    // 被删除的定时器在内部
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}
// 到期的定时器，调用其回调函数并将其从链表中删除
void sort_timer_lst::tick()
{
    if (!head)
    {
        return;
    }
    // 获取此刻的时间戳
    time_t cur = time(NULL);

    util_timer *tmp = head;
    while (tmp)
    {
        // 链表为升序排序，当前时间小于该定时器的超过时间，则后面的的定时器也没有到期
        if (cur < tmp->expires)
        {
            break;
        }
        // 当前定时器到期，则调用回调函数，执行定时时间
        tmp->cb_func(tmp->user_data);
        // 将处理后的定时器从链表容器中删除
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        // 重置头结点
        tmp = head;
    }
}
// 调整链表内部结点（增加）
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head)
{
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    // 遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置
    while (tmp)
    {
        if (timer->expires < tmp->expires)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    // 遍历完还未插入，则目标定时器放到尾结点
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}





// 设置超时时间
void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

/*
* 功能： 复制一个已经有的描述符（cmd=F_DUPFD或者F_DUPFD_CLOEXEC）
* 		获取/设置文件描述符标志（cmd=F_GETFD或者F_SETFD）
* 		获取/设置文件状态标志（cmd=F_GETFL或者F_SETFL）
* 		获取/设置异步IO所有权（cmd=F_GETOWN或者F_SETOWN）
*       获取/设置记录锁(cmd=F_GETLK或者F_SETLK或者F_SETLKW)
*/

// 对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL); // 设置文件描述符旧的状态标志
    int new_option = old_option | O_NONBLOCK; // 非阻塞模式
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/*
水平触发(level-triggered)

socket接收缓冲区不为空 有数据可读 读事件一直触发
socket发送缓冲区不满 可以继续写入数据 写事件一直触发
边沿触发(edge-triggered)

socket的接收缓冲区状态变化时触发读事件，即空的接收缓冲区刚接收到数据时触发读事件
socket的发送缓冲区状态变化时触发写事件，即满的缓冲区刚空出空间时触发写事件
边沿触发仅触发一次，水平触发会一直触发。
水平触发是只要读缓冲区有数据，就会一直触发可读信号，而边缘触发仅仅在空变为非空的时候通知一次，

事件宏

EPOLLIN ： 表示对应的文件描述符可以读（包括对端SOCKET正常关闭）；
EPOLLOUT： 表示对应的文件描述符可以写；
EPOLLPRI： 表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）；
EPOLLERR： 表示对应的文件描述符发生错误；
EPOLLHUP： 表示对应的文件描述符被挂断；
EPOLLET： 将 EPOLL设为边缘触发(Edge Triggered)模式（默认为水平触发），这是相对于水平触发(Level Triggered)来说的。
EPOLLONESHOT： 只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
*/

/*
水平触发和边缘触发模式区别
读缓冲区刚开始是空的
读缓冲区写入2KB数据
水平触发和边缘触发模式此时都会发出可读信号
收到信号通知后，读取了1kb的数据，读缓冲区还剩余1KB数据
水平触发会再次进行通知，而边缘触发不会再进行通知
所以，边缘触发需要一次性的把缓冲区的数据读完为止，也就是一直读，直到读到EGAIN为止，EGAIN说明缓冲区已经空了，因为这一点，边缘触发需要设置文件句柄为非阻塞
//水平触发
ret = read(fd, buf, sizeof(buf));

//边缘触发
while(true) {
    ret = read(fd, buf, sizeof(buf);
    if (ret == EAGAIN) break;
}

*/

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    // EPOLLRDHUP: 当Socket接受收到对方关闭连接时的请求之后触发
    if (1 == TRIGMode) // 边沿触发模式
        event.events = EPOLLIN || EPOLLET || EPOLLRDHUP;
    else              // 水平触发模式
        event.events = EPOLLIN || EPOLLRDHUP;
    // EPOLLONESHOT ：对于connfd,一个线程处理socket时，其他线程将无法处理
    // 当该线程处理完毕后，需要通过epoll_ctl重置EPOLLONESHOT事件
    if (one_shot)
        event.events |= EPOLLONESHOT;

    // 将内核事件表添加该事件
    /*
    第二个参数表示动作，用三个宏来表示：
    EPOLL_CTL_ADD：注册新的fd到epfd中；
    EPOLL_CTL_MOD：修改已经注册的fd的监听事件；
    EPOLL_CTL_DEL：从epfd中删除一个fd；
    */
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    // 对文件描述符设置为非阻塞
    setnonblocking(fd);
}

// 信号处理函数
// 可重入性：中断后再次进入该函数，环境变量与之前相同，不会丢失数据
void Utils::sig_handler(int sig)
{
    // Linux中系统调用的错误都存储在errno中，errno有操作系统维护，存储就近发生的错误
    // 为保证函数的可重入性，保留原来的error
    int save_errno = errno;
    int msg = sig;
    // 将信号值写入管道的写入端pipefd[1]，传输字符类型
    send(u_pipefd[1], (char *)&msg, 1, 0);

    // 将原来的errno赋值为当前的errno
    errno = save_errno;
}

// 设置需要捕捉的信号函数
void Utils::addsig(int sig, void (handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa)); // 初始化为0
    sa.sa_handler = handler; // 信号处理函数中仅仅发送挺信号量，不做对应逻辑处理
    if (restart)
        sa.sa_flags |= SA_RESTART;// 是否重新连接
    sigfillset(&sa.sa_mask); // 信号处理函数执行期间屏蔽所有信号
    assert(sigaction(sig, &sa, NULL) != -1);
}




// 定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();
    // 重定时以不断触发SIGALRM信号
    alarm(m_TIMESLOT);
}

// 相客户端发送错误信息
void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    // 关闭文件描述符
    close(connfd);
}

// 静态数据成员类外初始化
int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
// 定时器回调函数
void cb_func(client_data *user_data)
{
    // 删除非活动连接在Socket上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    // 关闭文件描述符
    close(user_data->sockfd);
    // 减少连接数
    http_conn::m_user_count--;
}