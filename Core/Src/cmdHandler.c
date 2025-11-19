#include "cmdHandler.h"
#include <string.h>
#include <stdio.h>

/* 命令格式定義  */
typedef struct {
	char cmdName[MAX_CMD_LEN];
	CommandCallback callback;
} Command_Typedef;

static Command_Typedef commands[MAX_CMD_COUNT] = {0}; //存放所有命令
static uint8_t cmdQty = 0; //目前命令數量

CmdHandlerStat_t register_command(const char *cmdName, CommandCallback callback) {
	if (cmdQty >= MAX_CMD_COUNT) {
		return QTY_OVER;
	} else if (cmdName == NULL) {
		return NAME_ERR;
	} else if (callback == NULL) {
		return CALBCK_ERR;
	}

	// 檢查是否重複
	for (uint8_t i = 0; i < cmdQty; ++i) {
		if (strcmp(commands[i].cmdName, cmdName) == 0) {
			//重複
			return REPEAT_ERR;
		}
	}

	strncpy(commands[cmdQty].cmdName, cmdName, MAX_CMD_LEN - 1);
	// commands[cmdQty].cmdName[MAX_CMD_LEN - 1] = '\0';
	commands[cmdQty].callback = callback;
	++cmdQty;
	return CMD_OK;
}

CmdHandlerStat_t execute_command(const char *cmd, ResStruct_t *_resStruct) {
	if (cmd == NULL) return CMD_ERR;

	char cmdName[MAX_CMD_LEN] = {0};
	size_t cmdLen;

	// 查找命令表中的匹配命令
	for (uint8_t i = 0; i < cmdQty; ++i) {
		cmdLen = strlen(commands[i].cmdName);
		if (strncmp(cmd, commands[i].cmdName, cmdLen) == 0) {
			strncpy(cmdName, cmd, cmdLen);
			cmdName[cmdLen] = '\0';
			printf("%-20s %s is running...\r\n", "[cmdhandler.c]", cmdName);
			commands[i].callback(cmd, _resStruct);
			// memset(cmdName, 0, MAX_CMD_LEN);
			return CMD_OK;
		}
	}
	printf("%-20s No matching command found for: %s\r\n", "[cmdhandler.c]", cmd);
	return EXC_ERR;
}

bool isReqCmd(const char *cmd) {
	if (cmd == NULL) return false;
	return (strstr(cmd, (const char *) "cReq") != NULL);
}

CmdHandlerStat_t isValidCmd(const char *cmd) {
	for (int i = 0; i < cmdQty; ++i) {
		if (0 == strncmp(commands[i].cmdName, cmd, strlen(commands[i].cmdName))) {
			return CMD_OK;
		}
	}
	return CMD_ERR;
}

bool extract_parameter(const char *input, char *output, size_t max_len) {
	if (!input || !output || max_len == 0) {
		return false;
	}

	// 找到 '<' 和 '>'
	char *start = strchr(input, '<');
	char *end = strchr(input, '>');

	// 檢查格式
	if (!start || !end || end <= start + 1) {
		return false;
	}

	// 計算參數長度
	size_t len = end - start - 1;
	if (len >= max_len) {
		len = max_len - 1; // 防止溢出
	}

	// 複製參數
	strncpy(output, start + 1, len);
	output[len] = '\0';

	// 移除 \r\n
	for (int i = 0; i < strlen(output); i++) {
		if (output[i] == '\r' || output[i] == '\n') {
			output[i] = '\0';
			break;
		}
	}

	return true;
}

#ifdef DEBUG
void print_all_cmd(void) {
	for (uint8_t i = 0; i < cmdQty; ++i) {
		printf("%-20s %s\n", commands[i].cmdName);
	}
}
#endif
