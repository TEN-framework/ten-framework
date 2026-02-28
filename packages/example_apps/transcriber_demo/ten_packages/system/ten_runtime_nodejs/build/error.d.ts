export declare enum TenErrorCode {
    ErrorCodeGeneric = 1,
    ErrorCodeInvalidJSON = 2,
    ErrorCodeInvalidArgument = 3,
    ErrorCodeInvalidType = 4,
    ErrorCodeInvalidGraph = 5,
    ErrorCodeTenIsClosed = 6,
    ErrorCodeMsgNotConnected = 7,
    ErrorCodeTimeout = 8
}
export declare class TenError {
    private _errorCode;
    private _errorMessage;
    constructor(errorCode: TenErrorCode, errorMessage: string);
    get errorCode(): TenErrorCode;
    get errorMessage(): string | undefined;
}
