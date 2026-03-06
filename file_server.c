#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>

/* 调试日志开关：设置为 1 启用调试日志，0 禁用 */
#define DEBUG 1

#if DEBUG
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG(level, fmt, ...) printf("[%s] [%s:%d %s] " fmt "\n", level, __FILENAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define LOGD(fmt, ...) LOG("DEBUG", fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOG("INFO", fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOG("WARN", fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) LOG("ERROR", fmt, ##__VA_ARGS__)
#else
#define LOGD(fmt, ...)
#define LOGI(fmt, ...)
#define LOGW(fmt, ...)
#define LOGE(fmt, ...)
#endif

#define PORT 8080
#define BUFFER_SIZE 65536
#define MAX_PATH 1024
#define MAX_URI 1024

static char *file_root = NULL;

typedef struct
{
    int client_fd;
    struct sockaddr_in client_addr;
} client_args_t;

/* 获取 HTTP 状态码描述 */
static const char *get_status_text(int status)
{
    switch (status)
    {
    case 200:
        return "OK";
    case 206:
        return "Partial Content";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 416:
        return "Range Not Satisfiable";
    case 500:
        return "Internal Server Error";
    default:
        return "Unknown";
    }
}

/* 获取文件的 MIME 类型 */
static const char *get_content_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (ext == NULL)
        return "application/octet-stream";

    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".js") == 0)
        return "application/javascript";
    if (strcmp(ext, ".json") == 0)
        return "application/json";
    if (strcmp(ext, ".txt") == 0)
        return "text/plain";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcmp(ext, ".ico") == 0)
        return "image/x-icon";
    if (strcmp(ext, ".svg") == 0)
        return "image/svg+xml";
    if (strcmp(ext, ".mp4") == 0)
        return "video/mp4";
    if (strcmp(ext, ".webm") == 0)
        return "video/webm";
    if (strcmp(ext, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(ext, ".wav") == 0)
        return "audio/wav";
    if (strcmp(ext, ".pdf") == 0)
        return "application/pdf";
    if (strcmp(ext, ".zip") == 0)
        return "application/zip";
    if (strcmp(ext, ".tar") == 0)
        return "application/x-tar";
    if (strcmp(ext, ".gz") == 0)
        return "application/gzip";
    if (strcmp(ext, ".xml") == 0)
        return "application/xml";

    return "application/octet-stream";
}

/* 获取当前时间字符串 (HTTP 格式) */
static void get_http_date(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    strftime(buf, size, "%a, %d %b %Y %H:%M:%S GMT", tm);
}

/* 获取文件最后修改时间 (HTTP 格式) */
static void get_file_mtime(struct stat *st, char *buf, size_t size)
{
    struct tm *tm = gmtime(&st->st_mtime);
    strftime(buf, size, "%a, %d %b %Y %H:%M:%S GMT", tm);
}

/* 发送 HTTP 响应头 */
static void send_response_header(int fd, int status, const char *content_type,
                                 long content_length, long range_start, long range_end,
                                 long file_size, struct stat *st)
{
    char header[BUFFER_SIZE];
    char date[64];
    char mtime[64];

    get_http_date(date, sizeof(date));
    get_file_mtime(st, mtime, sizeof(mtime));

    /* 判断是否为视频文件，如果是则添加 Content-Disposition: inline */
    int is_video = (strncmp(content_type, "video/", 6) == 0);
    const char *content_disposition = is_video ? "Content-Disposition: inline\r\n" : "";

    LOGD("Sending response header: status=%d (%s), content_type=%s, content_length=%ld",
         status, get_status_text(status), content_type, content_length);

    if (is_video)
    {
        LOGD("Video file detected, adding Content-Disposition: inline");
    }

    if (status == 206)
    {
        LOGI("Range response: bytes %ld-%ld/%ld", range_start, range_end, file_size);
    }

    int len;
    if (status == 206)
    {
        len = snprintf(header, sizeof(header),
                       "HTTP/1.1 206 %s\r\n"
                       "Date: %s\r\n"
                       "Server: FileServer/1.0\r\n"
                       "Content-Type: %s\r\n"
                       "%s"
                       "Content-Length: %ld\r\n"
                       "Content-Range: bytes %ld-%ld/%ld\r\n"
                       "Last-Modified: %s\r\n"
                       "Accept-Ranges: bytes\r\n"
                       "Connection: keep-alive\r\n"
                       "\r\n",
                       get_status_text(status), date, content_type, content_disposition,
                       content_length, range_start, range_end, file_size, mtime);
    }
    else
    {
        len = snprintf(header, sizeof(header),
                       "HTTP/1.1 %d %s\r\n"
                       "Date: %s\r\n"
                       "Server: FileServer/1.0\r\n"
                       "Content-Type: %s\r\n"
                       "%s"
                       "Content-Length: %ld\r\n"
                       "Last-Modified: %s\r\n"
                       "Accept-Ranges: bytes\r\n"
                       "Connection: keep-alive\r\n"
                       "\r\n",
                       status, get_status_text(status), date, content_type, content_disposition,
                       content_length, mtime);
    }

    LOGD("Header length: %d bytes", len);
    send(fd, header, len, 0);
}

/* 发送错误响应 */
static void send_error_response(int fd, int status)
{
    char body[BUFFER_SIZE];
    int len = snprintf(body, sizeof(body),
                       "<!DOCTYPE html>\n"
                       "<html><head><title>%d %s</title></head>\n"
                       "<body><h1>%d %s</h1></body></html>\n",
                       status, get_status_text(status),
                       status, get_status_text(status));

    struct stat st = {0};
    char date[64];
    get_http_date(date, sizeof(date));

    char header[512];
    int hlen = snprintf(header, sizeof(header),
                        "HTTP/1.1 %d %s\r\n"
                        "Date: %s\r\n"
                        "Server: FileServer/1.0\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: %d\r\n"
                        "Connection: keep-alive\r\n"
                        "\r\n",
                        status, get_status_text(status), date, len);

    send(fd, header, hlen, 0);
    send(fd, body, len, 0);
}

/* URL 解码 */
static void url_decode(char *dst, const char *src)
{
    char a, b;
    while (*src)
    {
        if (*src == '%' && (a = src[1]) && (b = src[2]) &&
            isxdigit(a) && isxdigit(b))
        {
            if (a >= 'a')
                a -= 'a' - 'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a' - 'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        }
        else if (*src == '+')
        {
            *dst++ = ' ';
            src++;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/* 解析 Range 头 */
static int parse_range_header(const char *range_header, long file_size,
                              long *start, long *end)
{
    if (range_header == NULL || strncmp(range_header, "bytes=", 6) != 0)
    {
        return -1;
    }

    const char *p = range_header + 6;
    while (*p == ' ')
        p++;

    if (*p == '-')
    {
        p++;
        long suffix = 0;
        while (isdigit(*p))
        {
            suffix = suffix * 10 + (*p - '0');
            p++;
        }
        *start = file_size - suffix;
        *end = file_size - 1;
        if (*start < 0)
            *start = 0;
        return 0;
    }

    *start = 0;
    while (isdigit(*p))
    {
        *start = *start * 10 + (*p - '0');
        p++;
    }

    if (*p != '-')
        return -1;
    p++;

    if (!isdigit(*p))
    {
        *end = file_size - 1;
    }
    else
    {
        *end = 0;
        while (isdigit(*p))
        {
            *end = *end * 10 + (*p - '0');
            p++;
        }
    }

    if (*start > *end || *start >= file_size)
        return -1;
    if (*end >= file_size)
        *end = file_size - 1;

    return 0;
}

/* 发送文件内容 */
static void send_file_content(int fd, const char *path, long start, long end)
{
    LOGI("send_file_content: path=%s, start=%ld, end=%ld, size=%ld", path, start, end, end - start + 1);

    int file_fd = open(path, O_RDONLY);
    if (file_fd < 0)
    {
        LOGE("Failed to open file: %s, error: %s", path, strerror(errno));
        send_error_response(fd, 500);
        return;
    }

    if (lseek(file_fd, start, SEEK_SET) < 0)
    {
        LOGE("Failed to seek file: %s, error: %s", path, strerror(errno));
        close(file_fd);
        send_error_response(fd, 500);
        return;
    }

    char buffer[BUFFER_SIZE];
    long remaining = end - start + 1;
    long total_sent = 0;
    int chunk_count = 0;
    time_t start_time = time(NULL);

    LOGI("Starting to send %ld bytes (%.2f MB)...", remaining, (double)remaining / (1024 * 1024));

    while (remaining > 0)
    {
        size_t to_read = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : remaining;
        ssize_t n = read(file_fd, buffer, to_read);

        if (n <= 0)
        {
            LOGW("File read ended early: n=%zd, remaining=%ld", n, remaining);
            break;
        }

        ssize_t sent = send(fd, buffer, n, 0);

        if (sent <= 0)
        {
            if (errno == EPIPE)
            {
                LOGI("Client closed connection (EPIPE), sent=%ld/%ld bytes", total_sent, end - start + 1);
            }
            else if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                LOGE("Send error: %s", strerror(errno));
            }
            break;
        }

        remaining -= n;
        total_sent += sent;
        chunk_count++;

        /* 每 1000 个块或最后一组数据打印进度 */
        if (chunk_count % 1000 == 0 || remaining == 0)
        {
            time_t elapsed = time(NULL) - start_time;
            double speed = elapsed > 0 ? (double)total_sent / elapsed / 1024 / 1024 : 0;
            LOGI("Progress: %ld/%ld bytes (%.1f%%), chunks: %d, elapsed: %lds, speed: %.1f MB/s",
                 total_sent, end - start + 1,
                 (float)total_sent / (end - start + 1) * 100,
                 chunk_count, elapsed, speed);
        }
    }

    LOGI("File sending completed: total_sent=%ld bytes, chunks=%d", total_sent, chunk_count);
    close(file_fd);
}

/* 从请求中提取头部值 */
static char *get_header_value(const char *request, const char *header_name)
{
    static char value[256];
    char search[128];

    snprintf(search, sizeof(search), "\r\n%s:", header_name);

    const char *pos = strstr(request, search);
    if (pos == NULL)
    {
        search[2] = toupper(search[2]);
        pos = strstr(request, search);
        if (pos == NULL)
            return NULL;
    }

    pos += strlen(search);
    while (*pos == ' ')
        pos++;

    int i = 0;
    while (*pos != '\r' && *pos != '\n' && *pos != '\0' && i < 255)
    {
        value[i++] = *pos++;
    }
    value[i] = '\0';

    return value;
}

/* 生成目录列表 */
static void send_directory_listing(int fd, const char *dir_path, const char *uri)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        send_error_response(fd, 500);
        return;
    }

    char body[BUFFER_SIZE * 4];
    int len = snprintf(body, sizeof(body),
                       "<!DOCTYPE html>\n"
                       "<html><head><meta charset=\"utf-8\"><title>Index of %s</title>\n"
                       "<style>body{font-family:monospace;} a{text-decoration:none;}"
                       "a:hover{text-decoration:underline;}</style></head>\n"
                       "<body><h1>Index of %s</h1><hr><pre>\n",
                       uri, uri);

    /* 添加上级目录链接 */
    if (strcmp(uri, "/") != 0)
    {
        len += snprintf(body + len, sizeof(body) - len,
                        "<a href=\"../\">../</a>\n");
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && len < (int)sizeof(body) - 256)
    {
        if (entry->d_name[0] == '.')
            continue;

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) < 0)
            continue;

        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&st.st_mtime));

        if (S_ISDIR(st.st_mode))
        {
            len += snprintf(body + len, sizeof(body) - len,
                            "<a href=\"%s/\">%-40s</a> %s %10s\n",
                            entry->d_name, entry->d_name, time_str, "-");
        }
        else
        {
            len += snprintf(body + len, sizeof(body) - len,
                            "<a href=\"%s\">%-40s</a> %s %10ld\n",
                            entry->d_name, entry->d_name, time_str, (long)st.st_size);
        }
    }

    closedir(dir);

    len += snprintf(body + len, sizeof(body) - len,
                    "</pre><hr></body></html>\n");

    struct stat st = {0};
    char date[64];
    get_http_date(date, sizeof(date));

    char header[512];
    int hlen = snprintf(header, sizeof(header),
                        "HTTP/1.1 200 OK\r\n"
                        "Date: %s\r\n"
                        "Server: FileServer/1.0\r\n"
                        "Content-Type: text/html; charset=utf-8\r\n"
                        "Content-Length: %d\r\n"
                        "Connection: keep-alive\r\n"
                        "\r\n",
                        date, len);

    send(fd, header, hlen, 0);
    send(fd, body, len, 0);
}

/* 构建文件路径并检查安全性 */
static int build_file_path(const char *uri, char *full_path, size_t size)
{
    char decoded_uri[MAX_URI];
    url_decode(decoded_uri, uri);

    /* 移除查询字符串 */
    char *query = strchr(decoded_uri, '?');
    if (query)
        *query = '\0';

    /* 构建完整路径 */
    int len = snprintf(full_path, size, "%s%s", file_root, decoded_uri);

    /* 安全检查：防止路径遍历攻击 */
    if (strstr(full_path, "..") != NULL)
    {
        return -1;
    }

    return len;
}

/* 处理 GET/HEAD 请求 */
static void handle_request(int fd, const char *method, const char *uri, const char *range_header)
{
    LOGI("handle_request: method=%s, uri=%s", method, uri);

    char full_path[MAX_PATH];

    if (build_file_path(uri, full_path, sizeof(full_path)) < 0)
    {
        LOGE("Path traversal attack detected: %s", uri);
        send_error_response(fd, 403);
        return;
    }

    LOGD("Full path: %s", full_path);

    struct stat st;
    if (stat(full_path, &st) < 0)
    {
        LOGE("File not found: %s", full_path);
        send_error_response(fd, 404);
        return;
    }

    LOGI("File size: %ld bytes (%.2f MB)", (long)st.st_size, (double)st.st_size / (1024 * 1024));

    /* 如果是目录，显示目录列表 */
    if (S_ISDIR(st.st_mode))
    {
        LOGD("Path is a directory");
        /* 尝试查找 index.html */
        char index_path[MAX_PATH];
        snprintf(index_path, sizeof(index_path), "%s/index.html", full_path);

        struct stat index_st;
        if (stat(index_path, &index_st) == 0 && S_ISREG(index_st.st_mode))
        {
            LOGD("Found index.html");
            strcpy(full_path, index_path);
            st = index_st;
        }
        else
        {
            if (strcmp(method, "GET") == 0)
            {
                send_directory_listing(fd, full_path, uri);
            }
            else
            {
                /* HEAD 请求：返回空目录列表响应 */
                send_error_response(fd, 200);
            }
            return;
        }
    }

    /* 检查是否为常规文件 */
    if (!S_ISREG(st.st_mode))
    {
        LOGE("Not a regular file: %s", full_path);
        send_error_response(fd, 403);
        return;
    }

    /* 解析 Range 头 */
    long start = 0;
    long end = st.st_size - 1;
    int is_range_request = 0;

    if (range_header != NULL)
    {
        LOGI("Range header: %s", range_header);
        if (parse_range_header(range_header, st.st_size, &start, &end) == 0)
        {
            is_range_request = 1;
            LOGI("Range request: bytes=%ld-%ld (of %ld)", start, end, (long)st.st_size);
        }
        else
        {
            LOGE("Invalid Range header");
            char response[BUFFER_SIZE];
            int len = snprintf(response, sizeof(response),
                               "HTTP/1.1 416 Range Not Satisfiable\r\n"
                               "Content-Range: bytes */%ld\r\n"
                               "Content-Length: 0\r\n\r\n",
                               (long)st.st_size);
            send(fd, response, len, 0);
            return;
        }
    }

    const char *content_type = get_content_type(full_path);
    long content_length = end - start + 1;

    LOGI("Content-Type: %s, Content-Length: %ld", content_type, content_length);

    /* 发送响应头 */
    send_response_header(fd, is_range_request ? 206 : 200, content_type,
                         content_length, start, end, st.st_size, &st);

    /* GET 请求发送内容 */
    if (strcmp(method, "GET") == 0)
    {
        send_file_content(fd, full_path, start, end);
    }
    else
    {
        LOGD("HEAD request, not sending content");
    }

    LOGI("Request completed successfully");
}

/* 处理客户端请求 */
void *handle_client(void *arg)
{
    client_args_t *args = (client_args_t *)arg;
    int client_fd = args->client_fd;
    struct sockaddr_in client_addr = args->client_addr;

    pthread_detach(pthread_self());
    free(args);

    char buffer[BUFFER_SIZE];

    printf("Connection from %s:%d (fd=%d)\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port), client_fd);

    /* 设置接收超时 */
    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int keep_alive = 1;
    int request_count = 0;

    while (keep_alive)
    {
        request_count++;
        LOGD("=== Waiting for request #%d (fd=%d) ===", request_count, client_fd);

        ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0)
        {
            if (n == 0)
            {
                LOGI("Client disconnected gracefully (fd=%d)", client_fd);
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                LOGI("Receive timeout (fd=%d), closing connection", client_fd);
            }
            else
            {
                LOGE("Receive error on fd=%d: %s", client_fd, strerror(errno));
            }
            break;
        }
        buffer[n] = '\0';

        LOGD("Received %zd bytes on fd=%d", n, client_fd);

        char method[16], uri[MAX_URI], version[16];
        if (sscanf(buffer, "%15s %1024s %15s", method, uri, version) != 3)
        {
            LOGE("Failed to parse request line on fd=%d", client_fd);
            send_error_response(client_fd, 400);
            break;
        }

        printf("fd=%d: %s %s %s\n", client_fd, method, uri, version);

        if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0)
        {
            LOGE("Method not allowed on fd=%d: %s", client_fd, method);
            char response[] = "HTTP/1.1 405 Method Not Allowed\r\n"
                              "Allow: GET, HEAD\r\n"
                              "Content-Length: 0\r\n"
                              "Connection: close\r\n\r\n";
            send(client_fd, response, sizeof(response) - 1, 0);
            break;
        }

        /* 获取头部值并立即复制 */
        char range_value[256] = {0};
        char connection_value[256] = {0};

        char *range_header = get_header_value(buffer, "Range");
        if (range_header)
        {
            strncpy(range_value, range_header, sizeof(range_value) - 1);
            range_header = range_value;
        }

        char *connection_header = get_header_value(buffer, "Connection");
        if (connection_header)
        {
            strncpy(connection_value, connection_header, sizeof(connection_value) - 1);
            connection_header = connection_value;
        }

        LOGD("fd=%d: Request headers - Range=%s, Connection=%s",
             client_fd,
             range_header ? range_header : "NULL",
             connection_header ? connection_header : "NULL");

        LOGI("=== Starting to handle request #%d on fd=%d ===", request_count, client_fd);
        handle_request(client_fd, method, uri, range_header);
        LOGI("=== Request #%d completed on fd=%d ===", request_count, client_fd);

        /* 检查 Connection 头 */
        if (connection_header && strstr(connection_header, "close"))
        {
            LOGD("Client requested Connection: close on fd=%d", client_fd);
            keep_alive = 0;
        }

        /* 检查是否是 HTTP/1.0 */
        if (strcmp(version, "HTTP/1.0") == 0)
        {
            char *conn = get_header_value(buffer, "Connection");
            if (!conn || !strstr(conn, "keep-alive"))
            {
                LOGD("HTTP/1.0 without keep-alive on fd=%d, closing", client_fd);
                keep_alive = 0;
            }
        }
    }

    LOGI("Closing client connection (fd=%d), total requests=%d", client_fd, request_count);
    close(client_fd);
    return NULL;
}

static void default_sigpipe(int signo) {
    LOGI("SIGPIPE received");
}

/* 忽略 SIGPIPE 信号 */
static void ignore_sigpipe() {
    struct sigaction sa;
    sa.sa_handler = default_sigpipe;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, NULL);
}

int main(int argc, char *argv[])
{
    // 忽略 SIGPIPE 信号
    ignore_sigpipe();

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <root_directory> [port]\n", argv[0]);
        fprintf(stderr, "Example: %s /path/to/files 8080\n", argv[0]);
        fprintf(stderr, "\nAccess files via: http://localhost:port/filename\n");
        exit(1);
    }

    file_root = realpath(argv[1], NULL);
    if (file_root == NULL)
    {
        perror("realpath");
        fprintf(stderr, "Directory not found: %s\n", argv[1]);
        exit(1);
    }

    int port = (argc > 2) ? atoi(argv[2]) : PORT;

    /* 检查是否为目录 */
    struct stat st;
    if (stat(file_root, &st) < 0 || !S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Not a directory: %s\n", file_root);
        free(file_root);
        exit(1);
    }

    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        close(server_fd);
        exit(1);
    }

    printf("File server started\n");
    printf("Root directory: %s\n", file_root);
    printf("Listening on port %d\n", port);
    printf("\nTest with:\n");
    printf("  curl http://localhost:%d/\n", port);
    printf("  curl http://localhost:%d/filename.mp4\n", port);
    printf("  curl -H \"Range: bytes=0-99\" http://localhost:%d/filename.mp4\n", port);
    printf("  wget -c http://localhost:%d/filename.mp4\n", port);

    while (1)
    {
        client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        client_args_t *args = malloc(sizeof(client_args_t));
        args->client_fd = client_fd;
        args->client_addr = client_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, args) != 0)
        {
            perror("pthread_create");
            close(client_fd);
            free(args);
        }
    }

    close(server_fd);
    free(file_root);
    return 0;
}
