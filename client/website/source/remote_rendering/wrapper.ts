import * as Module from "../../../wrapper/binary/wrapper";

export * from "../../../wrapper/binary/wrapper";
export type WrapperModule = Module.MainModule;

export function load_wrapper_module(options?: unknown) : Promise<WrapperModule>
{
    return Module.default(options);
}