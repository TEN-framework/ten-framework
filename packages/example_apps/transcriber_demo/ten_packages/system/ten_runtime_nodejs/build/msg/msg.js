import { Loc } from "../loc.js";
import ten_addon from "../ten_addon.js";
export class Msg {
    getName() {
        return ten_addon.ten_nodejs_msg_get_name(this);
    }
    getSource() {
        const arr = ten_addon.ten_nodejs_msg_get_source(this);
        return new Loc({
            appUri: arr[0],
            graphId: arr[1],
            extensionName: arr[2],
        });
    }
    setDests(dests) {
        const locs = dests.map((d) => new Loc(d));
        return ten_addon.ten_nodejs_msg_set_dests(this, locs);
    }
    setPropertyFromJson(path, jsonStr) {
        return ten_addon.ten_nodejs_msg_set_property_from_json(this, path, jsonStr);
    }
    getPropertyToJson(path) {
        return ten_addon.ten_nodejs_msg_get_property_to_json(this, path);
    }
    setPropertyNumber(path, value) {
        return ten_addon.ten_nodejs_msg_set_property_number(this, path, value);
    }
    getPropertyNumber(path) {
        return ten_addon.ten_nodejs_msg_get_property_number(this, path);
    }
    setPropertyString(path, value) {
        return ten_addon.ten_nodejs_msg_set_property_string(this, path, value);
    }
    getPropertyString(path) {
        return ten_addon.ten_nodejs_msg_get_property_string(this, path);
    }
    setPropertyBool(path, value) {
        return ten_addon.ten_nodejs_msg_set_property_bool(this, path, value);
    }
    getPropertyBool(path) {
        return ten_addon.ten_nodejs_msg_get_property_bool(this, path);
    }
    setPropertyBuf(path, value) {
        return ten_addon.ten_nodejs_msg_set_property_buf(this, path, value);
    }
    getPropertyBuf(path) {
        return ten_addon.ten_nodejs_msg_get_property_buf(this, path);
    }
}
//# sourceMappingURL=msg.js.map