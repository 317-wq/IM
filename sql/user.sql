-- M3: 用户表（account 登录，nickname 仅展示）
-- 表结构由 odb 生成（build/generated/user.sql）后整理而来；
-- 开发阶段可重复执行，会重建 user 表。

CREATE DATABASE IF NOT EXISTS `im`
  DEFAULT CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;
USE `im`;

DROP TABLE IF EXISTS `user`;

CREATE TABLE `user` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `user_id` varchar(64) NOT NULL,
  `account` varchar(32) NOT NULL,
  `nickname` varchar(64) NOT NULL,
  `password_hash` varchar(128) NOT NULL,
  `salt` varchar(32) NOT NULL,
  `description` TEXT NULL,
  `avatar_id` varchar(64) NULL,
  `created_at` TIMESTAMP NULL)
 ENGINE=InnoDB;

CREATE UNIQUE INDEX `user_id_i`
  ON `user` (`user_id`);

CREATE UNIQUE INDEX `account_i`
  ON `user` (`account`);

CREATE INDEX `nickname_i`
  ON `user` (`nickname`);
