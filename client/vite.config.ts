import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import basicSsl from '@vitejs/plugin-basic-ssl'
import path from "path";
import fs from "fs";

const server_ip = "localhost";
const server_port = 9000;
const client_port = 443;

export default defineConfig(
{
    root: "./website",
    plugins:
    [
        solidPlugin(),
        //basicSsl() //Self-Signed Certificat
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
        },
        https:
        {
            key: fs.readFileSync("/mnt/c/Users/tb397677/Desktop/Documents/certificates/private_key.pem"),
            cert: fs.readFileSync("/mnt/c/Users/tb397677/Desktop/Documents/certificates/certificat.pem")
        }
    }
});