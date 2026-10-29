-- M6: 消息表
CREATE DATABASE IF NOT EXISTS `im`
  DEFAULT CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;
USE `im`;

DROP TABLE IF EXISTS `message`;

CREATE TABLE `message` (
  `id` BIGINT UNSIGNED NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `message_id` varchar(64) NOT NULL,
  `session_id` varchar(64) NOT NULL,
  `sender_id` varchar(64) NOT NULL,
  `type` TINYINT UNSIGNED NOT NULL,
  `content` TEXT NULL,
  `file_id` varchar(64) NULL,
  `file_name` varchar(128) NULL,
  `file_size` INT UNSIGNED NULL,
  `created_at` TIMESTAMP NULL)
 ENGINE=InnoDB;

CREATE UNIQUE INDEX `message_id_i` ON `message` (`message_id`);
CREATE INDEX `message_session_time_i` ON `message` (`session_id`, `created_at`);
