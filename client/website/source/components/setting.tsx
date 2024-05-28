import { Component, JSX, createSignal, onMount } from "solid-js";
import { Button, Form, InputGroup } from "solid-bootstrap";
import SceneBrowser from "./scene_browser";
import FileBrowser from "./file_browser";
import { DirectoryFileType } from "../remote_rendering/connection";

export enum SettingNumberType
{
    Float,
    Int
}

export interface SettingDropdownProps
{
    label : string;
    children : JSX.Element;
    value : string;
    set_value : (value : string) => void;
}

export interface SettingNumberProps
{
    label : string;
    type? : SettingNumberType,
    min_value : number;
    max_value : number;
    step? : number;
    value : number;
    set_value : (value : number) => void;
}

export interface SettingFileProps
{
    label : string;
    select_type : DirectoryFileType;
    value : string;
    set_value : (value : string) => void;
}

export interface SettingSceneProps
{
    label : string;
    value : string;
    set_value : (value : string) => void;
}

export const SettingDropdown : Component<SettingDropdownProps> = (props) =>
{
    return (
        <div class="row align-items-center mx-3 bg-light rounded" style="margin-bottom: 0.75rem; padding: 0.75rem">
            <div class="col-3">
                <Form.Label class="col-form-label" for={props.label.toLowerCase()}>{props.label}</Form.Label>
            </div>
            <div class="col-9">
                <Form.Select id={props.label.toLowerCase()} value={props.value} onChange={event => props.set_value(event.target.value)}>
                    {props.children}
                </Form.Select>
            </div>           
        </div>
    );
};

export const SettingNumber : Component<SettingNumberProps> = (props) =>
{
    const [input, set_input] = createSignal<HTMLInputElement>();
    const [range, set_range] = createSignal<HTMLInputElement>();
    const type = props.type ?? SettingNumberType.Int;
    const step = props.step ?? 1;

    function get_value() : string
    {
        if(type == SettingNumberType.Float)
        {
            let value_string = props.value.toFixed(5);

            while(value_string.endsWith("0"))
            {
                if(value_string[value_string.length - 2] == ".")
                {
                    break;
                }   

                value_string = value_string.slice(0, -1);
            }

            return value_string;
        }

        return Math.round(props.value).toString();
    }

    function get_min_value() : string
    {
        if(type == SettingNumberType.Float)
        {
            return props.min_value.toFixed(1);
        }

        return props.min_value.toString();
    }

    function get_max_value() : string
    {
        if(type == SettingNumberType.Float)
        {
            return props.max_value.toFixed(1);
        }

        return props.max_value.toString();
    }

    function on_change(value_string : string | undefined)
    {
        if(value_string == undefined)
        {
            return;   
        }

        let value = parseFloat(value_string);

        if(type == SettingNumberType.Int)
        {
            value = Math.round(value);   
        }

        value = Math.min(Math.max(value, props.min_value), props.max_value);
        props.set_value(value);

        const current_input = input();
        const current_range = range();

        if(current_input != undefined)
        {
            current_input.value = get_value();   
        }

        if(current_range != undefined)
        {
            current_range.value = get_value();   
        }
    }

    onMount(() =>
    {
        const current_input = input();
        const current_range = range();

        if(current_input != undefined)
        {
            current_input.value = get_value();   
        }

        if(current_range != undefined)
        {
            current_range.value = get_value();   
        }
    });

    return (
        <div class="row align-items-stretch mx-3 bg-light rounded" style="margin-bottom: 0.75rem; padding: 0.75rem">
            <div class="col-3">
                <Form.Label class="col-form-label" for={props.label.toLowerCase()}>{props.label}</Form.Label>
            </div>
            <div class="col-7">
                <div class="h-100" style="position: relative">
                    <Form.Range ref={set_range} class="h-100" id={props.label.toLowerCase()} value={get_value()} min={props.min_value} max={props.max_value} step={step} onChange={event => on_change(event.target.value)}></Form.Range>
                    <div style="position: absolute; z-index: 100; left: 0px; bottom: -10px;">{get_min_value()}</div>
                    <div style="position: absolute; z-index: 100; right: 0px; bottom: -10px;">{get_max_value()}</div>
                </div>
            </div>
            <div class="col-2">
                <Form.Control ref={set_input} type="number" value={get_value()} min={props.min_value} max={props.max_value} step={props.step} onChange={event => on_change(event.target.value)} onKeyPress={event => on_change(input()?.value)}></Form.Control>
            </div>
        </div>
    );  
};

export const SettingFile : Component<SettingFileProps> = (props) =>
{
    const [show_browser, set_show_browser] = createSignal(false);

    return (
        <div class="row align-items-stretch mx-3 bg-light rounded" style="margin-bottom: 0.75rem; padding: 0.75rem">
            <div class="col-3">
                <Form.Label class="col-form-label" for={props.label.toLowerCase()}>{props.label}</Form.Label>
            </div>
            <div class="col-9">
                <InputGroup>
                    <Form.Control type="text" value={props.value} onChange={event => props.set_value(event.target.value)}></Form.Control>
                    <Button onClick={event => set_show_browser(true)}>Browse</Button>
                </InputGroup>
            </div>
            <FileBrowser show={show_browser} set_show={set_show_browser} set_select={props.set_value} select_type={props.select_type}></FileBrowser>
        </div>
    );
};

export const SettingScene : Component<SettingSceneProps> = (props) =>
{
    const [show_browser, set_show_browser] = createSignal(false);

    return (
        <div class="row align-items-stretch mx-3 bg-light rounded" style="margin-bottom: 0.75rem; padding: 0.75rem">
            <div class="col-3">
                <Form.Label class="col-form-label" for={props.label.toLowerCase()}>{props.label}</Form.Label>
            </div>
            <div class="col-9">
                <InputGroup>
                    <Form.Control type="text" value={props.value} onChange={event => props.set_value(event.target.value)}></Form.Control>
                    <Button onClick={event => set_show_browser(true)}>Browse</Button>
                </InputGroup>
            </div>
            <SceneBrowser show={show_browser} set_show={set_show_browser} set_select={props.set_value}></SceneBrowser>
        </div>
    );
};