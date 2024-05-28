import { mat4, vec2, vec3 } from "gl-matrix";
import { log_error, log_info } from "./log";

export enum DisplayType
{
    Desktop,
    AR
}

export type OnDisplayRender = () => void;
export type OnDisplayClose = () => void;

export interface Display
{
    create(calibrate : boolean) : Promise<boolean>;
    destroy() : void;
    show() : Promise<void>; //Only after the promise resolves, all parameters of the display are valid

    set_on_render(callback : OnDisplayRender) : void;
    set_on_close(callback : OnDisplayClose) : void;

    get_resolution() : vec2;
    get_projection_matrix() : mat4;
    get_view_matrix() : mat4;
    get_position() : vec3;
    get_type() : DisplayType;
}

export function build_display(type : DisplayType, canvas : HTMLCanvasElement, gl : WebGL2RenderingContext) : Display | null
{
    switch(type)
    {
    case DisplayType.Desktop:
        return new DesktopDisplay(canvas);
    case DisplayType.AR:
        return new ARDisplay(canvas, gl);
    default:
        break;
    }

    return null;
}

const DESKTOP_DISPLAY_VIEW_UPDATE_RATE = 16; //In milliseconds
const DESKTOP_DISPLAY_MOVEMENT_SPEED = 1.0;
const DESKTOP_DISPLAY_ROTATION_SPEED = 0.025;
const DESKTOP_DISPLAY_FIELD_OF_VIEW = 80.0;
const DESKTOP_DISPLAY_NEAR_DISTANCE = 0.1;
const DESKTOP_DISPLAY_FAR_DISTANCE = 200.0;

class DesktopDisplay
{
    private canvas : HTMLCanvasElement;
    private frame_request : number | null = null;
    private view_interval : number | null = null;
    private resize_observer : ResizeObserver | null = null;
    private close_button_element : HTMLDivElement | null = null;

    private resolution = vec2.create();
    private projection_matrix = mat4.create();
    private view_matrix = mat4.create();
    private position = vec3.create();
    private vertical_angle = 0.0;
    private horizontal_angle = -Math.PI;
    
    private input_forward = false;
    private input_backward = false;
    private input_left = false;
    private input_right = false;
    private input_up = false;
    private input_down = false;

    private on_render : OnDisplayRender | null = null;
    private on_close : OnDisplayClose | null = null;

    constructor(canvas: HTMLCanvasElement)
    {
        this.canvas = canvas;
    }

    async create(calibrate : boolean) : Promise<boolean>
    {
        this.canvas.onmousemove = this.on_mouse_move.bind(this);
        this.canvas.onkeydown = this.on_key_down.bind(this);
        this.canvas.onkeyup = this.on_key_up.bind(this);

        this.resolution[0] = this.canvas.clientWidth;
        this.resolution[1] = this.canvas.clientHeight;
        this.canvas.width = this.resolution[0];
        this.canvas.height = this.resolution[1];

        this.resize_observer = new ResizeObserver(this.on_resize.bind(this));
        this.resize_observer.observe(this.canvas);

        this.setup_close_button();

        vec3.zero(this.position);
        this.compute_projection_matrix();
        this.compute_view_matrix();
        
        this.view_interval = setInterval(this.compute_view_matrix.bind(this), DESKTOP_DISPLAY_VIEW_UPDATE_RATE);

        return true;
    }

    destroy()
    {
        this.on_render = null;
        this.on_close = null;

        if(this.close_button_element != null)
        {
            this.close_button_element.remove();
            this.close_button_element = null;   
        }

        if(this.resize_observer != null)
        {
            this.resize_observer.disconnect();
            this.resize_observer = null;   
        }

        if(this.frame_request != null)
        {
            cancelAnimationFrame(this.frame_request);   
        }

        if(this.view_interval != null)
        {
            clearInterval(this.view_interval);   
        }
    }

    show() : Promise<void>
    {
        if(this.frame_request != null)
        {
            return Promise.resolve();
        }

        const render_function = () =>
        {
            if(this.on_render != null)
            {
                this.on_render();

                this.frame_request = requestAnimationFrame(render_function.bind(this)); 
            }

            else
            {
                this.frame_request = null;
            }
        };

        this.frame_request = requestAnimationFrame(render_function.bind(this));

        return Promise.resolve();
    }

    set_on_render(callback : OnDisplayRender)
    {
        this.on_render = callback;
    }

    set_on_close(callback : OnDisplayClose)
    {
        this.on_close = callback;
    }

    get_resolution() : vec2
    {
        return this.resolution;
    }

    get_projection_matrix() : mat4
    {
        return this.projection_matrix;
    }

    get_view_matrix() : mat4
    {
        return this.view_matrix;
    }

    get_position() : vec3
    {
        return this.position;
    }

    get_type() : DisplayType
    {
        return DisplayType.Desktop;
    }

    private compute_projection_matrix()
    {
        const aspect_ratio = this.canvas.clientWidth / this.canvas.clientHeight;   
        mat4.perspective(this.projection_matrix, (DESKTOP_DISPLAY_FIELD_OF_VIEW / 180.0) * Math.PI, aspect_ratio, DESKTOP_DISPLAY_FAR_DISTANCE, DESKTOP_DISPLAY_NEAR_DISTANCE); //Switch far and near distance due to mat4.perspective() implementation

        //mat4.perspective() creates an left-handed projection matrix that needs to be converted to right-handed
        this.projection_matrix[10] = -this.projection_matrix[10];
        this.projection_matrix[14] = -this.projection_matrix[14];
    }

    private compute_view_matrix()
    {
        const forward = vec3.fromValues(0,0,-1);
        const side = vec3.fromValues(1,0,0);
        const up = vec3.fromValues(0,1,0);

        const delta_position = vec3.create();
        const delta_time = 1.0 / DESKTOP_DISPLAY_VIEW_UPDATE_RATE;
        
        if(this.input_forward)
        {
            vec3.scaleAndAdd(delta_position, delta_position, forward, delta_time * DESKTOP_DISPLAY_MOVEMENT_SPEED);
        }
        
        if(this.input_backward)
        {
            vec3.scaleAndAdd(delta_position, delta_position, forward, -delta_time * DESKTOP_DISPLAY_MOVEMENT_SPEED);
        }
        
        if(this.input_left)
        {
            vec3.scaleAndAdd(delta_position, delta_position, side, -delta_time * DESKTOP_DISPLAY_MOVEMENT_SPEED);
        }
        
        if(this.input_right)
        {
            vec3.scaleAndAdd(delta_position, delta_position, side, delta_time * DESKTOP_DISPLAY_MOVEMENT_SPEED);
        }
        
        if(this.input_up)
        {
            vec3.scaleAndAdd(delta_position, delta_position, up, delta_time * DESKTOP_DISPLAY_MOVEMENT_SPEED);
        }
        
        if(this.input_down)
        {
            vec3.scaleAndAdd(delta_position, delta_position, up, -delta_time * DESKTOP_DISPLAY_MOVEMENT_SPEED);
        }
        
        vec3.rotateY(delta_position, delta_position, vec3.fromValues(0.0, 0.0, 0.0), -this.horizontal_angle);
        vec3.add(this.position, this.position, delta_position);

        let translation = vec3.create();
        vec3.negate(translation, this.position);

        mat4.identity(this.view_matrix);
        mat4.rotateX(this.view_matrix, this.view_matrix, this.vertical_angle);
        mat4.rotateY(this.view_matrix, this.view_matrix, this.horizontal_angle);
        mat4.translate(this.view_matrix, this.view_matrix, translation);
    }

    private setup_close_button()
    {
        this.close_button_element = document.createElement("div");
        this.close_button_element.className = "rounded-circle";
        this.close_button_element.style.position = "absolute";
        this.close_button_element.style.top = "32px";
        this.close_button_element.style.right = "32px";
        this.close_button_element.style.zIndex = "200";
        this.close_button_element.style.background = "rgba(255, 255, 255, 0.5)";
        this.close_button_element.style.display = "flex";
        this.close_button_element.style.alignItems = "center";
        this.close_button_element.style.justifyContent = "center";
        this.close_button_element.style.padding = "0.5rem";
        
        this.canvas.parentElement?.insertBefore(this.close_button_element, this.canvas);

        const button = document.createElement("button");
        button.className = "btn-close";
        button.onclick = this.on_close_internal.bind(this);

        this.close_button_element.appendChild(button);
    }

    private on_resize(entries: ResizeObserverEntry[])
    {
        if(entries.length == 0)
        {
            return;   
        }

        const canvas_entry = entries[0];
        this.resolution[0] = canvas_entry.contentBoxSize[0].inlineSize;
        this.resolution[1] = canvas_entry.contentBoxSize[0].blockSize;
        this.canvas.width = this.resolution[0];
        this.canvas.height = this.resolution[1];

        this.compute_projection_matrix();
    }

    private on_mouse_move(event : MouseEvent)
    {
        if((event.buttons & 0x01) != 0)
        {
            this.horizontal_angle += event.movementX * DESKTOP_DISPLAY_ROTATION_SPEED;
            this.vertical_angle += event.movementY * DESKTOP_DISPLAY_ROTATION_SPEED;
            
            if(this.horizontal_angle >= 0.0)
            {
                this.horizontal_angle = this.horizontal_angle % (Math.PI * 2.0);
            }

            else
            {
                this.horizontal_angle = (Math.PI * 2.0) - (this.horizontal_angle % (Math.PI * 2.0));
            }

            this.vertical_angle = Math.min(Math.max(this.vertical_angle, -Math.PI / 2.0), Math.PI / 2.0);
        }
    }

    private on_key_down(event : KeyboardEvent)
    {
        switch(event.code)
        {
        case "KeyW":
            this.input_forward = true;
            break;
        case "KeyA":
            this.input_left = true;
            break;
        case "KeyS":
            this.input_backward = true;
            break;
        case "KeyD":
            this.input_right= true;
            break;
        case "Space":
            this.input_up = true;
            break;
        case "ShiftLeft":
            this.input_down = true;
            break;
        default:
            break;
        }
    }

    private on_key_up(event : KeyboardEvent)
    {
        switch(event.code)
        {
        case "KeyW":
            this.input_forward = false;
            break;
        case "KeyA":
            this.input_left = false;
            break;
        case "KeyS":
            this.input_backward = false;
            break;
        case "KeyD":
            this.input_right= false;
            break;
        case "Space":
            this.input_up = false;
            break;
        case "ShiftLeft":
            this.input_down = false;
            break;
        default:
            break;
        }
    }

    private on_close_internal()
    {
        if(this.on_close == null)
        {
            return;
        }

        this.on_close();
    }
}

const AR_DISPLAY_NEAR_DISTANCE = 0.1;
const AR_DISPLAY_FAR_DISTANCE = 200.0;
const AR_DISPLAY_FLOOR_OFFSET = 1.5;        //Distance of the device to the floor in meters when calibrating the origin
const AR_DISPLAY_WEBXR_VIEWER_FIXES = true;

class ARDisplay
{
    private canvas : HTMLCanvasElement;
    private gl : WebGL2RenderingContext;
    private session : XRSystem | null = null;
    private reference_space : XRReferenceSpace | null = null;
    private base_layer : XRWebGLLayer | null = null;
    private frame_request : number | null = null;

    private overlay_element : HTMLDivElement | null = null;
    private close_button_element : HTMLDivElement | null = null;
    private calibration_element : HTMLDivElement | null = null;

    private calibration_origin : vec3 = vec3.create();
    private calibration_side_x : vec3 = vec3.create();
    private calibration_complete : boolean = false;

    private resolution = vec2.create();
    private projection_matrix = mat4.create();
    private view_matrix = mat4.create();
    private position = vec3.create();

    private on_render : OnDisplayRender | null = null;
    private on_close : OnDisplayClose | null = null;

    constructor(canvas : HTMLCanvasElement, gl : WebGL2RenderingContext)
    {
        this.canvas = canvas;
        this.gl = gl;
    }

    async create(calibrate : boolean) : Promise<boolean>
    {
        if(!("xr" in navigator) || navigator.xr == null)
        {
            log_info("[ARDisplay] WebXR feature not supported!");

            return false;
        }

        this.overlay_element = document.createElement("div");
        this.overlay_element.id = "ar-display-overlay";
        this.canvas.parentElement?.insertBefore(this.overlay_element, this.canvas);

        const session_options =       
        {
            requiredFeatures: ["dom-overlay"],
            domOverlay:
            {
                root: AR_DISPLAY_WEBXR_VIEWER_FIXES ? "ar-display-overlay" : this.overlay_element //WebXR Viewer requires a string not an object
            }
        };

        this.session = await navigator.xr.requestSession("immersive-ar", session_options).catch((error : any) =>
        {
            return null;
        });

        if(this.session == null)
        {
            log_error("[ARDisplay] WebXR does not support AR mode!");

            return false;   
        }

        this.reference_space = await this.session.requestReferenceSpace("local").catch((error : any) =>
        {
            return null;
        });

        if(this.reference_space == null)
        {
            log_error("[ARDisplay] Can't create reference space local!");

            return false;
        }

        const context_response = await this.gl.makeXRCompatible().catch((error) =>
        {
            return false;
        })

        if(context_response != undefined)
        {
            log_error("[ARDisplay] Can't make rendering context XR compatible!");

            return false;
        }

        //WebXR Viewer does not take into account the framebuffer scaling factor. 
        //Manually set the framebuffer size by changing the with and height of the canvas
        this.canvas.width = this.canvas.clientWidth;
        this.canvas.height = this.canvas.clientHeight;

        const base_layer_scaling = XRWebGLLayer.getNativeFramebufferScaleFactor(this.session);

        this.base_layer = new XRWebGLLayer(this.session, this.gl,
        {
            framebufferScaleFactor: base_layer_scaling //Render with native render resolution
        });

        if(this.base_layer == null)
        {
            log_error("[ARDisplay] Can't create base layer!");

            return false;
        }

        const render_state =
        {
            baseLayer: this.base_layer,
            depthNear: AR_DISPLAY_NEAR_DISTANCE,
            depthFar: AR_DISPLAY_FAR_DISTANCE,
        };

        this.session.updateRenderState(render_state);
        this.session.addEventListener("end", this.on_close_internal.bind(this));

        this.resolution[0] = this.base_layer.framebufferWidth;
        this.resolution[1] = this.base_layer.framebufferHeight;

        this.setup_close_button();

        if(calibrate)
        {
            this.setup_calibrate_origin();
        }

        return true;
    }

    async destroy()
    {
        this.on_render = null;
        this.on_close = null;

        if(AR_DISPLAY_WEBXR_VIEWER_FIXES)
        {
            //For some reason the WebXR Viewer changes the background color of the website. 
            //Or the backround color is not restored correctly.
            document.body.style.background = "white";
        }

        if(this.overlay_element != null)
        {
            this.overlay_element.remove();
            this.overlay_element = null;

            this.close_button_element = null;
            this.calibration_element = null;
        }

        if (this.session != null)
        {
            if(this.frame_request != null)
            {
                this.session.cancelAnimationFrame(this.frame_request);
                this.frame_request = null;
            }

            await this.session.end();

            this.base_layer = null;
            this.reference_space = null;
            this.session = null;
        }
    }

    show() : Promise<void>
    {
        if(this.frame_request != null)
        {
            return Promise.resolve();
        }

        const setup_complete = new Promise<void>(resolve =>
        {
            const render_function = (time: number, frame: XRFrame) =>
            {    
                this.compute_projection_matrix(frame);
                this.compute_view_matrix(frame);
    
                if(this.on_render != null)
                {
                    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, this.base_layer.framebuffer);
                    this.on_render();
                    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, null);
    
                    this.frame_request = this.session.requestAnimationFrame(render_function);
                }
    
                else
                {
                    this.frame_request = null;
                }

                resolve();
            };
    
            this.frame_request = this.session.requestAnimationFrame(render_function);
        });

        return setup_complete;
    }

    set_on_render(callback : OnDisplayRender)
    {
        this.on_render = callback;
    }

    set_on_close(callback : OnDisplayClose)
    {
        this.on_close = callback;
    }

    get_resolution() : vec2
    {
        return this.resolution;
    }

    get_projection_matrix() : mat4
    {
        return this.projection_matrix;
    }

    get_view_matrix() : mat4
    {
        return this.view_matrix;
    }

    get_position() : vec3
    {
        return this.position;
    }

    get_type() : DisplayType
    {
        return DisplayType.AR;
    }

    private compute_projection_matrix(frame: XRFrame)
    {
        const pose = frame.getViewerPose(this.reference_space);

        if(pose == null)
        {
            return;
        }

        const view = pose.views[0];
        mat4.copy(this.projection_matrix, view.projectionMatrix);
    }

    private compute_view_matrix(frame: XRFrame)
    {
        const pose = frame.getViewerPose(this.reference_space);

        if(pose == null)
        {
            return;
        }

        const calibration_matrix = mat4.create();
        const calibration_matrix_inverse = mat4.create();
        mat4.identity(calibration_matrix);
        mat4.identity(calibration_matrix_inverse);

        if(this.calibration_complete)
        {
            const origin = this.calibration_origin;
            const side_x = this.calibration_side_x;

            const side_direction = vec3.create();
            vec3.sub(side_direction, side_x, origin);
            side_direction[1] = 0.0;

            const forward_direction = vec3.create();
            forward_direction[0] = -side_direction[2];
            forward_direction[1] = 0.0;
            forward_direction[2] = side_direction[0];

            vec3.normalize(side_direction, side_direction);
            vec3.normalize(forward_direction, forward_direction);

            calibration_matrix[0] = side_direction[0];
            calibration_matrix[1] = side_direction[1];
            calibration_matrix[2] = side_direction[2];
            calibration_matrix[8] = forward_direction[0];
            calibration_matrix[9] = forward_direction[1];
            calibration_matrix[10] = forward_direction[2];
            calibration_matrix[12] = origin[0];
            calibration_matrix[13] = origin[1] + AR_DISPLAY_FLOOR_OFFSET;
            calibration_matrix[14] = origin[2];

            mat4.invert(calibration_matrix_inverse, calibration_matrix);
        }

        const view = pose.views[0];
        mat4.copy(this.view_matrix, view.transform.inverse.matrix);
        mat4.multiply(this.view_matrix, this.view_matrix, calibration_matrix_inverse);

        this.position[0] = view.transform.position.x;
        this.position[1] = view.transform.position.y;
        this.position[2] = view.transform.position.z;
        vec3.transformMat4(this.position, this.position, calibration_matrix);
    }

    private setup_close_button()
    {
        if(this.overlay_element == null)
        {
            return;    
        }

        this.close_button_element = document.createElement("div");
        this.close_button_element.className = "rounded-circle";
        this.close_button_element.style.position = "absolute";
        this.close_button_element.style.zIndex = "300";
        this.close_button_element.style.top = "32px";
        this.close_button_element.style.right = "32px";
        this.close_button_element.style.background = "rgba(255, 255, 255, 0.5)";
        this.close_button_element.style.display = "flex";
        this.close_button_element.style.alignItems = "center";
        this.close_button_element.style.justifyContent = "center";
        this.close_button_element.style.padding = "0.5rem";

        this.overlay_element.appendChild(this.close_button_element);

        const button = document.createElement("button");
        button.className = "btn-close";
        button.onclick = this.on_close_internal.bind(this);

        this.close_button_element.appendChild(button);
    }

    private setup_calibrate_origin()
    {
        if(this.overlay_element == null)
        {
            return;
        }

        this.calibration_element = document.createElement("div");
        this.calibration_element.style.position = "absolute";
        this.calibration_element.style.zIndex = "200";
        this.calibration_element.style.left = "0px";
        this.calibration_element.style.right = "0px";
        this.calibration_element.style.top = "0px";
        this.calibration_element.style.bottom = "0px";
        this.calibration_element.style.background = "black";
        this.calibration_element.style.display = "flex";
        this.calibration_element.style.flexDirection = "column";
        this.calibration_element.style.justifyContent = "center";
        this.calibration_element.style.alignItems = "center";
        this.calibration_element.style.padding = "48px";

        this.overlay_element.appendChild(this.calibration_element);

        const description = document.createElement("p");
        description.innerText = "Please move the device to the position that should be the origin of the virtual environment. Besides that, make sure to hold the device roughly at chest height when pushing confirm.";
        description.style.color = "white";
        description.style.maxWidth = "256px";

        this.calibration_element.appendChild(description);

        const button = document.createElement("button");
        button.className = "btn btn-primary"; //Use Boostrap
        button.innerText = "Confirm";
        button.style.marginTop = "48px";
        button.onclick = this.on_calibrate_origin.bind(this);

        this.calibration_element.appendChild(button);
    }

    private setup_calibrate_side_x()
    {
        if(this.overlay_element == null)
        {
            return;
        }

        this.calibration_element = document.createElement("div");
        this.calibration_element.style.position = "absolute";
        this.calibration_element.style.zIndex = "200";
        this.calibration_element.style.left = "0px";
        this.calibration_element.style.right = "0px";
        this.calibration_element.style.top = "0px";
        this.calibration_element.style.bottom = "0px";
        this.calibration_element.style.background = "black";
        this.calibration_element.style.display = "flex";
        this.calibration_element.style.flexDirection = "column";
        this.calibration_element.style.justifyContent = "center";
        this.calibration_element.style.alignItems = "center";
        this.calibration_element.style.padding = "48px";

        this.overlay_element.appendChild(this.calibration_element);

        const description = document.createElement("p");
        description.innerText = "Please move the device along the x-axis of the virtual environment and press confirm.";
        description.style.color = "white";
        description.style.maxWidth = "256px";

        this.calibration_element.appendChild(description);

        const button = document.createElement("button");
        button.className = "btn btn-primary"; //Use Boostrap
        button.innerText = "Confirm";
        button.style.marginTop = "48px";
        button.onclick = this.on_calibrate_side_x.bind(this);

        this.calibration_element.appendChild(button);
    }

    private on_calibrate_origin()
    {
        if(this.calibration_element == null)
        {
            return;
        }

        vec3.copy(this.calibration_origin, this.position);

        this.calibration_element.remove();
        this.calibration_element = null;
        
        this.setup_calibrate_side_x();
    }

    private on_calibrate_side_x()
    {
        if(this.calibration_element == null)
        {
            return;
        }

        vec3.copy(this.calibration_side_x, this.position);

        this.calibration_element.remove();
        this.calibration_element = null;
        this.calibration_complete = true;
    }

    private on_close_internal()
    {
        if(this.on_close == null)
        {
            return;   
        }

        this.on_close();
    }
}