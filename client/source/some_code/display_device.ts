import { mat4, vec3 } from "gl-matrix";

export enum DisplayDeviceType
{
    Desktop,
    AR
}

export type OnDisplayDeviceRender = () => boolean;
export type OnDisplayDeviceClose = () => void;

export interface DisplayDevice
{
    async create(canvas: HTMLCanvasElement) : Promise<boolean>;
    destroy() : void;

    set_on_render(callback : OnDisplayDeviceRender) : void;
    set_on_close(callback : OnDisplayDeviceClose) : void;

    get_projection_matrix() : mat4;
    get_view_matrix() : mat4;
    get_type() : DisplayDeviceType;
}

export function build_display_device(type : DisplayDeviceType) : DisplayDevice | null
{
    switch(type)
    {
    case DisplayDeviceType.Desktop:
        return new DesktopDisplay();
    case DisplayDeviceType.AR:
        return new ARDisplay();
    default:
        break;
    }

    return null;
}

const DESKTOP_DISPLAY_VIEW_UPDATE_RATE = 16; //In milliseconds
const DESKTOP_DISPLAY_MOVEMENT_SPEED = 1.5;
const DESKTOP_DISPLAY_ROTATION_SPEED = 0.05;
const DESKTOP_DISPLAY_FIELD_OF_VIEW = 80.0;
const DESKTOP_DISPLAY_NEAR_DISTANCE = 0.1;
const DESKTOP_DISPLAY_FAR_DISTANCE = 200.0;

class DesktopDisplay
{
    private canvas : HTMLCanvasElement | null = null;
    private context : WebGL2RenderingContext | null = null;
    private frame_request : number | null = null;
    private view_interval : number | null = null;

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

    private on_render : OnRenderCallback | null = null;
    private on_close : OnDisplayDeviceClose | null = null;

    async create(canvas: HTMLCanvasElement) : Promise<boolean>
    {
        this.canvas = canvas;
        this.context = canvas.getContext("webgl2");

        if(this.context == null)
        {
            return false;   
        }

        this.canvas.onresize = this.on_resize;
        this.canvas.onmousemove = this.on_mouse_move;
        this.canvas.onkeydown = this.on_key_down;
        this.canvas.onkeyup = this.on_key_up

        vec3.zero(this.position);
        this.compute_projection_matrix();
        this.compute_view_matrix();
        
        this.view_interval = setInterval(this.compute_view_matrix, DESKTOP_DISPLAY_VIEW_UPDATE_RATE);

        return true;
    }

    destroy() : void
    {
        if(this.frame_request != null)
        {
            cancelAnimationFrame(this.frame_request);   
        }

        if(this.view_interval != null)
        {
            clearInterval(this.view_interval);   
        }
    }

    set_on_render(callback : OnRenderCallback) : void
    {
        this.on_render = callback;

        const render_function = () =>
        {


            if(this.on_render())
            {
                this.frame_request = requestAnimationFrame(render_function); 
            }

            else
            {
                this.
            }
        };

        this.frame_request = requestAnimationFrame(render_function);
    }

    set_on_close(callback : OnDisplayDeviceClose) : void
    {
        this.on_close = callback;
    }

    get_projection_matrix() : mat4
    {
        return this.projection_matrix;
    }

    get_view_matrix() : mat4
    {
        return this.view_matrix;
    }

    get_type() : DisplayDeviceType
    {
        return DisplayDeviceType.Desktop;
    }

    private compute_projection_matrix()
    {
        if(this.canvas == null)
        {
            return;
        }

        const aspect_ratio = this.canvas.width / this.canvas.height;   
        mat4.perspective(this.projection_matrix, DESKTOP_DISPLAY_FIELD_OF_VIEW, aspect_ratio, DESKTOP_DISPLAY_NEAR_DISTANCE, DESKTOP_DISPLAY_FAR_DISTANCE);
    }

    private compute_view_matrix()
    {
        const forward = vec3.fromValues(0,0,1);
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
        
        vec3.rotateY(delta_position, delta_position, vec3.fromValues(0.0, 0.0, 0.0), this.horizontal_angle);
        vec3.add(this.position, this.position, delta_position);

        let translation = vec3.create();
        vec3.negate(translation, this.position);

        mat4.identity(this.view_matrix);
        mat4.rotateX(this.view_matrix, this.view_matrix, this.vertical_angle);
        mat4.rotateY(this.view_matrix, this.view_matrix, this.horizontal_angle);
        mat4.translate(this.view_matrix, this.view_matrix, translation)
    }

    private on_resize(even : UIEvent)
    {
        this.compute_projection_matrix();
    }

    private on_mouse_move(event : MouseEvent)
    {
        if(event.button == 0)
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
        switch(event.key)
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
        case "Shift":
            this.input_down = true;
            break;
        default:
            break;
        }
    }

    private on_key_up(event : KeyboardEvent)
    {
        switch(event.key)
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
        case "Shift":
            this.input_down = false;
            break;
        default:
            break;
        }
    }
}

const AR_DISPLAY_NEAR_DISTANCE = 0.1;
const AR_DISPLAY_FAR_DISTANCE = 200.0;

class ARDisplay
{
    private canvas : HTMLCanvasElement | null = null;
    private context : WebGL2RenderingContext | null = null;
    private session : XRSystem | null = null;
    private reference_space : XRReferenceSpace | null = null;
    private frame_request : number | null = null;

    private projection_matrix = mat4.create();
    private view_matrix = mat4.create();

    private on_render : OnRenderCallback | null = null;
    private on_close : OnCloseCallback | null = null;

    async create(canvas: HTMLCanvasElement) : Promise<boolean>
    {
        this.canvas = canvas;
        this.context = canvas.getContext("webgl2");

        if(this.context == null)
        {
            return false;   
        }

        if(!("xr" in navigator) || navigator.xr == null)
        {
            return false;
        }

        this.session = await navigator.xr.requestSession("immersive-ar").catch((error : any) =>
        {
            return null;
        });

        if(this.session == null)
        {
            return false;   
        }

        this.reference_space = await this.session.requestReferenceSpace("local").catch((error : any) =>
        {
            return null;
        });

        if(this.reference_space == null)
        {
            return false;   
        }

        const render_state =
        {
            baseLayer: new XRWebGLLayer(this.session, this.context),
            depthNear: AR_DISPLAY_NEAR_DISTANCE,
            depthFar: AR_DISPLAY_FAR_DISTANCE,
        };

        this.session.updateRenderState(render_state);

        this.session.addEventListener("end", () =>
        {
            if(this.on_close != null)
            {
                this.on_close();
            }
        });





        navigator.xr.requestSession("immersive-ar").then(session =>
        {
            this.session = session;
            this.session.addEventListener("end", () => this.xrSession = undefined);
        });


        /*



session.addEventListener("end", () => this.xrSession = undefined);

                    session.updateRenderState({
                        baseLayer: new XRWebGLLayer(session, gl),
                        depthNear: settings.session.nearPlane,
                        depthFar: settings.session.farPlane,
                    });
                    
                    session.requestReferenceSpace("local").then(refSpace => {
                        this.refSpace = refSpace;
                        session.requestAnimationFrame((t: number, f: any) => this.onXRAnimationFrame(t, f));
                    });

        */




        return true;
    }

    destroy() : void
    {
        if (this.session != null)
        {
            this.xrSession.cancelAnimationFrame(this.animationFrameRequest);
            this.xrSession.end();
        }

    }

    set_on_render(callback : OnRenderCallback) : void
    {
        
    }

    set_on_close(callback : OnCloseCallback) : void
    {
        this.on_close = callback;
    }

    get_projection_matrix() : mat4
    {
        return this.projection_matrix;
    }

    get_view_matrix() : mat4
    {
        return this.view_matrix;
    }

    get_type() : DisplayDeviceType
    {
        return DisplayDeviceType.AR;
    }
}