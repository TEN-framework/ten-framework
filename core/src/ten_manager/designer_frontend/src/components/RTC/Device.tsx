import { TDeviceSelectItem } from "@/types/rtc";
import {
    Select,
    SelectContent,
    SelectItem,
    SelectTrigger,
    SelectValue,
} from "@/components/ui/Select";


export const DeviceSelect = (props: {
    items: TDeviceSelectItem[]
    value: string
    onChange: (value: string) => void
    placeholder?: string
}) => {
    const { items, value, onChange, placeholder } = props;

    return (
        <Select value={value} onValueChange={onChange}>
            <SelectTrigger className="w-[290px]">
                <SelectValue placeholder={placeholder} />
            </SelectTrigger>
            <SelectContent>
                {items.map((item) => (
                    <SelectItem key={item.value} value={item.value}>
                        {item.label}
                    </SelectItem>
                ))}
            </SelectContent>
        </Select>
    );
};