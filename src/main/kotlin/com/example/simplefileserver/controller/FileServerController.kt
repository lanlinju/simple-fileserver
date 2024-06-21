package com.example.simplefileserver.controller

import jakarta.servlet.http.HttpServletRequest
import jakarta.servlet.http.HttpServletResponse
import org.springframework.beans.factory.annotation.Autowired
import org.springframework.beans.factory.annotation.Value
import org.springframework.web.bind.annotation.RequestMapping
import org.springframework.web.bind.annotation.RestController
import java.io.File
import java.net.URLConnection
import java.net.URLDecoder
import java.nio.charset.StandardCharsets

@RestController
@RequestMapping("/file")
class FileServerController @Autowired constructor(
    private val appConfig: AppConfig,
) {

    @Value("\${rootPath}")
    private lateinit var rootPath: String  // 设置的文件根目录

    @RequestMapping("/**")
    fun handleFileRequest(request: HttpServletRequest, response: HttpServletResponse) {
        if (appConfig.environment == "prod") {
            rootPath = appConfig.directory // 如果使用jar包启动时，从命令行获取目录
        }

        val path: String = rootPath + request.requestURI.replace("/file", "")
        val file = File(URLDecoder.decode(path, StandardCharsets.UTF_8))

        if (!file.exists()) {
            sendNotFound(response)
            return
        }

        if (file.isDirectory) {
            sendDirectoryList(file, response)
        } else {
            serveFile(file, request, response)
        }
    }

    private fun sendNotFound(response: HttpServletResponse) {
        response.status = HttpServletResponse.SC_NOT_FOUND
        response.writer.println("HTTP/1.0 404 Not Found")
    }

    private fun serveFile(file: File, request: HttpServletRequest, response: HttpServletResponse) {
        val mimeType = getMimeType(file)
        response.contentType = mimeType
        response.setContentLengthLong(file.length())

        // 处理文件的类型
        val contentDisposition = if (isInlineDisplay(mimeType) && appConfig.inlineDisplay) "inline" else "attachment"
        response.setHeader("Content-Disposition", "$contentDisposition; filename=\"${file.name}\"")

        val rangeRequest = request.getHeader("Range") != null // 检查是否是分片请求

        if (rangeRequest) {
            serveFileInChunks(file, request, response)
        } else {
            serveFileNormally(file, response)
        }
    }

    /**
     * 普通下载方式
     */
    private fun serveFileNormally(file: File, response: HttpServletResponse) {
        val output = response.outputStream
        val input = file.inputStream()
        val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
        var bytesRead: Int
        while (input.read(buffer).also { bytesRead = it } != -1) {
            output.write(buffer, 0, bytesRead)
        }
        input.close()
    }

    /**
     * 分片下载方式
     *
     * --- Request Header ---
     * GET /example-file.txt HTTP/1.1
     * Host: www.example.com
     * Range: bytes=0-499
     *
     * --- Response Header ---
     * HTTP/1.1 206 Partial Content
     * Content-Range: bytes 0-499/1234 // start-end/size, 0-/1234 equals 0-1234/1234
     * Accept-Ranges: bytes
     * Content-Length: 500 // 注意此时content-length为：end - start + 1
     * Content-Type: text/plain
     */
    private fun serveFileInChunks(file: File, request: HttpServletRequest, response: HttpServletResponse) {
        val (start, end) = getContentRange(request, file)
        val contentLength = end - start + 1

        response.setStatus(HttpServletResponse.SC_PARTIAL_CONTENT)
        response.setContentLengthLong(contentLength)
        response.setHeader("Accept-Ranges", "bytes")
        response.setHeader("Content-Range", "bytes $start-$end/${file.length()}")

        val output = response.outputStream
        val input = file.inputStream()
        input.skip(start)
        val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
        var bytesToRead = contentLength

        while (bytesToRead > 0) {
            val bytesRead = input.read(buffer)
            if (bytesRead > bytesToRead) {
                output.write(buffer, 0, bytesToRead.toInt()) // eg. 0-100/144, bytesToRead = 101, bytesRead = 145
                break
            }
            output.write(buffer, 0, bytesRead)
            bytesToRead -= bytesRead
        }
        input.close()
    }

    /**
     * 返回分片范围
     *
     * Range: bytes=0-499
     *
     * start = 0
     * end = 499
     */
    private fun getContentRange(request: HttpServletRequest, file: File): Pair<Long, Long> {
        val rangeHeader = request.getHeader("Range") ?: return Pair(0, file.length() - 1)
        val ranges = rangeHeader.removePrefix("bytes=").split("-") // 0-144 to ["0", "144"]; 0- to ["0", ""]
        val start = ranges[0].toLong()
        val end = if (ranges[1].isNotEmpty()) ranges[1].toLong() else file.length() - 1
        return Pair(start, end)
    }

    /**
     * 返回文件的Mime类型
     */
    private fun getMimeType(file: File): String {
        val mimeType = URLConnection.guessContentTypeFromName(file.name) ?: "application/octet-stream"
        return mimeType
    }

    /**
     * 视频和图片文件直接在浏览器展示
     */
    private fun isInlineDisplay(mimeType: String): Boolean {
        return mimeType.startsWith("image/") || mimeType.startsWith("video/")
    }

    /**
     * 文件列表
     *
     * 手机浏览器查看时显示字体过小的问题
     * @source https://blog.csdn.net/liji_digital/article/details/130546598
     */
    private fun sendDirectoryList(directory: File, response: HttpServletResponse) {
        response.contentType = "text/html; charset=UTF-8"

        val writer = response.writer

        val itemList = getItemList(directory)

        val htmlText = """
            <html>
                <head>
                    <title>${directory.name}</title>
                    <meta name="viewport" content="width=device-width, user-scalable=yes, initial-scale=0.8, maxmum-scale=1.0, minimum-scale=0.3">
                </head>
                <body>
                    <h1>${directory.name}</h1>
                    <ul>
                        ${itemList}
                    </ul>
                </body>
            </html>
        """.trimIndent()

        writer.print(htmlText)

    }

    /**
     * e.g.:
     * <li><a href="filename">filename</a></li>\n
     * <li><a href="filename">filename</a></li>\n\n
     */
    private fun getItemList(directory: File): StringBuilder {
        val itemList = StringBuilder()
        val files = directory.listFiles()
        for (file in files!!) {
            var filename = file.name

            if (file.isDirectory) {
                filename += "/"
            }

            val item = """<li><a href="$filename">$filename</a></li>"""
            itemList.append(item).append("\n")
        }
        return itemList
    }
}