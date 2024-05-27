import { Component, JSX, createSignal } from "solid-js";
import { Button, Form, InputGroup } from "solid-bootstrap";
import SceneBrowser from "./scene_browser";
import FileBrowser from "./file_browser";
import { DirectoryFileType } from "../remote_rendering/connection";

export interface SettingDropdownProps
{
    label : string;
    children : JSX.Element;
}

export interface SettingNumberProps
{
    label : string;
}

export interface SettingFileProps
{
    label : string;
    select_type : DirectoryFileType;
}

export interface SettingSceneProps
{
    label : string;
}

export const SettingDropdown : Component<SettingDropdownProps> = (props) =>
{
    return (
        <div class="row align-items-center mx-3 bg-light rounded" style="margin-bottom: 0.75rem; padding: 0.75rem">
            <div class="col-3">
                <Form.Label class="col-form-label" for={props.label.toLowerCase()}>{props.label}</Form.Label>
            </div>
            <div class="col-9">
                <Form.Select id={props.label.toLowerCase()}>
                    {props.children}
                </Form.Select>
            </div>           
        </div>
    );
};

export const SettingNumber : Component<SettingNumberProps> = (props) =>
{
    return (
        <div class="row align-items-stretch mx-3 bg-light rounded" style="margin-bottom: 0.75rem; padding: 0.75rem">
            <div class="col-3">
                <Form.Label class="col-form-label" for={props.label.toLowerCase()}>{props.label}</Form.Label>
            </div>
            <div class="col-7">
                <div class="h-100" style="position: relative">
                    <Form.Range class="h-100" id={props.label.toLowerCase()}></Form.Range>
                    <div style="position: absolute; z-index: 100; left: 0px; bottom: -10px;">0.01</div>
                    <div style="position: absolute; z-index: 100; right: 0px; bottom: -10px;">200.0</div>
                </div>
            </div>
            <div class="col-2">
                <Form.Control type="number"></Form.Control>
            </div>
        </div>
    );  
};

export const SettingFile : Component<SettingFileProps> = (props) =>
{
    const [show_browser, set_show_browser] = createSignal(false);
    const [file, set_file] = createSignal<string>();

    return (
        <div class="row align-items-stretch mx-3 bg-light rounded" style="margin-bottom: 0.75rem; padding: 0.75rem">
            <div class="col-3">
                <Form.Label class="col-form-label" for={props.label.toLowerCase()}>{props.label}</Form.Label>
            </div>
            <div class="col-9">
                <InputGroup>
                    <Form.Control type="text" value={file()} onChange={event => set_file(event.target.value)}></Form.Control>
                    <Button onClick={event => set_show_browser(true)}>Browse</Button>
                </InputGroup>
            </div>
            <FileBrowser show={show_browser} set_show={set_show_browser} set_select={set_file} select_type={props.select_type}></FileBrowser>
        </div>
    );
};

export const SettingScene : Component<SettingSceneProps> = (props) =>
{
    const [show_browser, set_show_browser] = createSignal(false);
    const [file, set_file] = createSignal<string>();

    return (
        <div class="row align-items-stretch mx-3 bg-light rounded" style="margin-bottom: 0.75rem; padding: 0.75rem">
            <div class="col-3">
                <Form.Label class="col-form-label" for={props.label.toLowerCase()}>{props.label}</Form.Label>
            </div>
            <div class="col-9">
                <InputGroup>
                    <Form.Control type="text" value={file()} onChange={event => set_file(event.target.value)}></Form.Control>
                    <Button onClick={event => set_show_browser(true)}>Browse</Button>
                </InputGroup>
            </div>
            <SceneBrowser show={show_browser} set_show={set_show_browser} set_select={set_file}></SceneBrowser>
        </div>
    );
};