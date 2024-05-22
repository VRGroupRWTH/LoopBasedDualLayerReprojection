import { send_log } from "./connection";

const LOG_FILE_NAME = "client_log.txt";
const LOG_ENABLE_DEBUG = true;
const LOG_ENABLE_INFO = true;
const LOG_ENABLE_ERROR = true;

export function log_debug(message : string)
{
    if(LOG_ENABLE_INFO)
    {
        log_string("[DEBUG] " + message);   
    }
}

export function log_info(message : string)
{
    if(LOG_ENABLE_DEBUG)
    {
        log_string("[INFO] " + message);   
    }
}

export function log_error(message : string)
{
    if(LOG_ENABLE_ERROR)
    {
        log_string("[Error] " + message);   
    }
}

function log_string(string : string)
{
    const date = new Date();
    let date_string = "[";
    date_string += date.getFullYear() + "-";
    date_string += date.getMonth() + "-";
    date_string += date.getDay() + " ";
    date_string += date.getHours() + ":";
    date_string += date.getMinutes() + ":";
    date_string += date.getSeconds() + ".";
    date_string += date.getMilliseconds() + "] ";

    console.log(date_string + string);

    const text_encoder = new TextEncoder();
    const text_buffer = text_encoder.encode(date_string + string + "\n");

    send_log(LOG_FILE_NAME, text_buffer);
}

function on_error(event: Event | string, source?: string, line_number?: number, column_number?: number, error?: Error)
{
    log_error(event.toString());
}

window.onerror = on_error;