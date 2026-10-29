-- M4: 好友关系、好友申请、会话、会话成员
-- 与 odb/*.hxx 对应；开发阶段可重复执行，会重建这四张表。

CREATE DATABASE IF NOT EXISTS `im`
  DEFAULT CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;
USE `im`;

DROP TABLE IF EXISTS `chat_session_member`;
DROP TABLE IF EXISTS `chat_session`;
DROP TABLE IF EXISTS `friend_apply`;
DROP TABLE IF EXISTS `relation`;

CREATE TABLE `relation` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `user_id` varchar(64) NOT NULL,
  `peer_id` varchar(64) NOT NULL,
  `created_at` TIMESTAMP NULL)
 ENGINE=InnoDB;

CREATE UNIQUE INDEX `relation_pair_i` ON `relation` (`user_id`, `peer_id`);
CREATE INDEX `relation_peer_i` ON `relation` (`peer_id`);

CREATE TABLE `friend_apply` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `event_id` varchar(64) NOT NULL,
  `from_user` varchar(64) NOT NULL,
  `to_user` varchar(64) NOT NULL,
  `created_at` TIMESTAMP NULL)
 ENGINE=InnoDB;

CREATE UNIQUE INDEX `friend_apply_event_i` ON `friend_apply` (`event_id`);
CREATE UNIQUE INDEX `friend_apply_pair_i` ON `friend_apply` (`from_user`, `to_user`);
CREATE INDEX `friend_apply_to_i` ON `friend_apply` (`to_user`);

CREATE TABLE `chat_session` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `session_id` varchar(64) NOT NULL,
  `name` varchar(64) NOT NULL,
  `type` TINYINT UNSIGNED NOT NULL,
  `created_at` TIMESTAMP NULL)
 ENGINE=InnoDB;

CREATE UNIQUE INDEX `chat_session_id_i` ON `chat_session` (`session_id`);

CREATE TABLE `chat_session_member` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `session_id` varchar(64) NOT NULL,
  `user_id` varchar(64) NOT NULL,
  `joined_at` TIMESTAMP NULL)
 ENGINE=InnoDB;

CREATE UNIQUE INDEX `session_member_i` ON `chat_session_member` (`session_id`, `user_id`);
CREATE INDEX `member_user_i` ON `chat_session_member` (`user_id`);
