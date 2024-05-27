import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import basicSsl from '@vitejs/plugin-basic-ssl'
import path from "path";

const server_ip = "192.168.178.22";
const server_port = 9000;
const client_port = 3000;

export default defineConfig(
{
    root: "./website",
    plugins:
    [
        solidPlugin(),
        basicSsl()
    ],
    define: 
    {
        server_ip: JSON.stringify(server_ip),
        server_port: JSON.stringify(server_port),
        client_port: JSON.stringify(client_port)
    },
    resolve:
    {
        alias:
        {
            "~bootstrap": path.resolve(__dirname, "node_modules/bootstrap"),
        }
    },
    server:
    {
        port: client_port,
        strictPort: true,
        proxy: 
        {
            "/server":
            {
                target: "http://" + server_ip + ":" + server_port,
                changeOrigin: true,
                rewrite: path => path.replace(/^\/server/, '')
            },
            "/server-socket":
            {
                target: "ws://" + server_ip + ":" + server_port,
                changeOrigin: true,
                ws: true,
                rewrite: path => path.replace(/^\/server-socket/, '')
            }
        },
        watch:
        {
            usePolling: true
        }
    }
});